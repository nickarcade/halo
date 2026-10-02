#!/usr/bin/env python3
"""Deterministic raw-XBE byte-improvement gate for one translation unit.

This tool deliberately does not rewrite C.  It records a freshly compiled per-function raw-XBE byte
baseline, classifies score-context diagnostics, and accepts a candidate only
when every selected target improves without regressing any other scored
function or introducing diagnostics.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT))
_SCORE_RE = re.compile(
    r"(?:PASS|FAIL)\s+(\S+):\s+([\d.]+)%\s+(?:mnemonic\s+)?match\s+"
    r"\((\d+)/(\d+)\s+insns\)"
    r"(?:.*?\|\s+opnd\s+([\d.]+)%\s+\(operand-normalized\))?"
)
_REGPARM_RE = re.compile(r"\[REGPARM\]\s+(\S+):")
_WARNING_CATEGORIES = {
    "fpu": "fpu_operand_order",
    "fcom": "fpu_guard",
    "loadw": "load_width",
    "imm": "immediate_constant",
}


class ScoreImproveError(RuntimeError):
    pass


def _relative(path, root):
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except ValueError:
        return str(path)


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _warning_count(warnings):
    return sum(len(values) for values in warnings.values())


def classify_context(context, regparm=False):
    """Return stable categories derived solely from verifier evidence."""
    categories = set()
    warnings = context.get("warnings", {})
    for key, category in _WARNING_CATEGORIES.items():
        if warnings.get(key):
            categories.add(category)

    frame = context.get("frame", {})
    if frame.get("cand_frame_bytes") != frame.get("ref_frame_bytes"):
        categories.add("frame_layout")
    if regparm:
        categories.add("register_abi")

    scores = context.get("scores", {})
    if scores.get("n_cand_insns") != scores.get("n_ref_insns"):
        categories.add("instruction_count")
    if not categories:
        categories.add("codegen_shape")
    return sorted(categories)


def _parse_scores(output, worktree):
    regparm = set(_REGPARM_RE.findall(output))
    scores = {}
    for match in _SCORE_RE.finditer(output):
        name, score, candidate, reference, operand = match.groups()
        context_path = worktree / "artifacts" / "score_context" / (name + ".json")
        context = {}
        if context_path.exists():
            try:
                context = json.loads(context_path.read_text())
            except json.JSONDecodeError as exc:
                raise ScoreImproveError(
                    "invalid score context for %s: %s" % (name, exc)
                )
        warnings = context.get("warnings", {})
        scores[name] = {
            "score": float(score),
            "operand_score": float(operand) if operand is not None else None,
            "candidate_instructions": int(candidate),
            "reference_instructions": int(reference),
            "warnings": warnings,
            "categories": classify_context(context, name in regparm),
        }
    if not scores:
        raise ScoreImproveError("vc71_verify produced no parsed function scores")
    return scores


def measure_mnemonic(source, worktree):
    source_path = (worktree / source).resolve()
    verifier = worktree / "tools" / "verify" / "vc71_verify.py"
    if not source_path.is_file():
        raise ScoreImproveError("source does not exist: %s" % source_path)
    if not verifier.is_file():
        raise ScoreImproveError("vc71 verifier does not exist: %s" % verifier)

    proc = subprocess.run(
        [sys.executable, str(verifier), _relative(source_path, worktree)],
        cwd=str(worktree), text=True, capture_output=True, check=False,
    )
    # vc71_verify exits 1 when any function scores below its PASS threshold
    # (default 50%). That is still a valid measurement for baseline/gating
    # purposes; only treat the run as failed when no scores can be parsed.
    if proc.returncode != 0:
        try:
            scores = _parse_scores(proc.stdout, worktree)
        except ScoreImproveError:
            scores = None
        if not scores:
            raise ScoreImproveError(
                "vc71_verify failed:\n%s%s" % (proc.stdout, proc.stderr))
    else:
        scores = _parse_scores(proc.stdout, worktree)
    return {
        "schema": 1,
        "source": _relative(source_path, worktree),
        "source_sha256": _sha256(source_path),
        "scores": scores,
    }


def compare_mnemonic(baseline, current, targets, min_improvement):
    """Return an acceptance report for a candidate measurement."""
    baseline_scores = baseline.get("scores", {})
    current_scores = current.get("scores", {})
    report = {
        "targets": sorted(targets),
        "improvements": {},
        "score_regressions": {},
        "warning_regressions": {},
        "missing": [],
    }

    for name in sorted(baseline_scores):
        if name not in current_scores:
            report["missing"].append(name)
            continue
        before = baseline_scores[name]
        after = current_scores[name]
        delta = after["score"] - before["score"]
        if name in targets:
            if delta >= min_improvement:
                report["improvements"][name] = delta
            else:
                report["score_regressions"][name] = {
                    "before": before["score"], "after": after["score"],
                    "required_improvement": min_improvement,
                }
        elif delta < 0:
            report["score_regressions"][name] = {
                "before": before["score"], "after": after["score"],
            }

        before_warnings = _warning_count(before.get("warnings", {}))
        after_warnings = _warning_count(after.get("warnings", {}))
        if after_warnings > before_warnings:
            report["warning_regressions"][name] = {
                "before": before_warnings, "after": after_warnings,
            }

    for target in sorted(targets):
        if target not in baseline_scores and target not in report["missing"]:
            report["missing"].append(target)

    report["passed"] = not (
        report["score_regressions"]
        or report["warning_regressions"]
        or report["missing"]
    )
    return report


def measure(source, worktree):
    """Fresh byte snapshot; mnemonic scores never enter acceptance decisions."""
    from tools.verify import byte_regression as byte_gate
    source_path = (worktree / source).resolve()
    if not source_path.is_file():
        raise ScoreImproveError("source does not exist: %s" % source_path)
    parent = worktree / "artifacts/score_improve"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="bytes-", dir=parent))
    plan = output / "plan.json"
    byte_gate.write_json(plan, {"sources": [_relative(source_path, worktree)]})
    snapshot = output / "snapshot.json"
    commit = byte_gate.git(worktree, "rev-parse", "HEAD")
    proc = subprocess.run([sys.executable, str(worktree / "tools/verify/byte_regression.py"),
                           "measure", "--commit", commit, "--run-id", uuid.uuid4().hex,
                           "--plan", str(plan), "--output", str(snapshot), "--allow-dirty"],
                          cwd=worktree)
    if proc.returncode:
        raise ScoreImproveError("raw-XBE byte measurement failed; artifacts: " + str(output))
    measurement = _read_json(snapshot)
    measurement["source"] = _relative(source_path, worktree)
    return measurement


def compare(baseline, current, targets, min_improvement):
    """Gate bytes and reject stale shared inputs or legacy mnemonic baselines."""
    from tools.verify import byte_regression as byte_gate
    errors = []
    metric = "raw_xbe_aligned_byte_lower_bound"
    if baseline.get("metric") != metric or current.get("metric") != metric:
        return {"passed": False, "errors": ["Re-record baseline: expected fresh raw-XBE bytes, not mnemonic scores"]}
    source = baseline.get("source")
    if not source or source != current.get("source"):
        errors.append("baseline source differs from candidate source")
    before_inputs = {k: v for k, v in baseline.get("inputs", {}).items() if k != source}
    after_inputs = {k: v for k, v in current.get("inputs", {}).items() if k != source}
    if before_inputs != after_inputs or baseline.get("environment") != current.get("environment"):
        errors.append("baseline measurement inputs are stale; shared inputs or toolchain changed")
    def by_address(snapshot):
        result = {}
        for record in snapshot.get("records", []):
            address = int(record["address"], 0)
            if address in result:
                raise ScoreImproveError("duplicate measured address")
            result[address] = record
        return result
    before, after = by_address(baseline), by_address(current)
    report = byte_gate.compare(before, after)
    errors.extend(report["errors"])
    improvements = {}
    for target in targets:
        addresses = [addr for addr, item in before.items() if item["function"] == target]
        if len(addresses) != 1 or addresses[0] not in after:
            errors.append(target + ": missing or ambiguous baseline/candidate")
            continue
        addr = addresses[0]
        old, new = byte_gate.counts(before[addr]), byte_gate.counts(after[addr])
        if old is None or new is None:
            errors.append(target + ": no valid byte measurement")
            continue
        delta = 100 * (new[0] / new[1] - old[0] / old[1])
        if delta < min_improvement:
            errors.append(target + ": byte improvement below required %.4f percentage points" % min_improvement)
        else:
            improvements[target] = delta
    report.update({"passed": not errors, "errors": errors, "improvements": improvements, "metric": metric})
    return report


def _read_json(path):
    try:
        return json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise ScoreImproveError("cannot read %s: %s" % (path, exc))


def _write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def cmd_baseline(args):
    measurement = measure(args.source, args.worktree)
    _write_json(args.output, measurement)
    print("baseline: %d function(s) -> %s" % (len(measurement["records"]), args.output))
    return 0


def cmd_categorize(args):
    context = _read_json(args.context)
    print(json.dumps({"categories": classify_context(context)}, indent=2))
    return 0


def cmd_check(args):
    baseline = _read_json(args.baseline)
    report = compare(
        baseline, measure(args.source, args.worktree), set(args.target),
        args.min_improvement,
    )
    if args.output:
        _write_json(args.output, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["passed"] else 1


def cmd_trial(args):
    proc = subprocess.run(args.candidate_cmd, cwd=str(args.worktree), shell=True)
    if proc.returncode != 0:
        print("candidate command failed: %d" % proc.returncode, file=sys.stderr)
        return proc.returncode
    return cmd_check(args)


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    def common(command):
        command.add_argument("--source", required=True, help="C source relative to --worktree")
        command.add_argument("--worktree", type=Path, default=REPO_ROOT,
                             help="Candidate worktree (default: repository root)")

    baseline = subparsers.add_parser("baseline", help="Record fresh raw-XBE aligned byte measurements")
    common(baseline)
    baseline.add_argument("--output", type=Path, required=True)
    baseline.set_defaults(handler=cmd_baseline)

    categorize = subparsers.add_parser("categorize", help="Classify one score-context JSON file")
    categorize.add_argument("--context", type=Path, required=True)
    categorize.set_defaults(handler=cmd_categorize)

    check = subparsers.add_parser("check", help="Gate a candidate against a baseline")
    common(check)
    check.add_argument("--baseline", type=Path, required=True)
    check.add_argument("--target", action="append", required=True,
                       help="Function whose byte accuracy must gain at least --min-improvement pp")
    check.add_argument("--min-improvement", type=float, default=0.01)
    check.add_argument("--output", type=Path)
    check.set_defaults(handler=cmd_check)

    trial = subparsers.add_parser("trial", help="Run a candidate command, then apply check")
    common(trial)
    trial.add_argument("--baseline", type=Path, required=True)
    trial.add_argument("--target", action="append", required=True)
    trial.add_argument("--min-improvement", type=float, default=0.01)
    trial.add_argument("--output", type=Path)
    trial.add_argument("--candidate-cmd", required=True,
                       help="Command run inside --worktree before scoring")
    trial.set_defaults(handler=cmd_trial)
    return parser


def main():
    args = build_parser().parse_args()
    if hasattr(args, "worktree"):
        args.worktree = args.worktree.resolve()
    try:
        return args.handler(args)
    except ScoreImproveError as exc:
        print("score_improve: %s" % exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
