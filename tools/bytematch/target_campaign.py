#!/usr/bin/env python3
"""Target-only deterministic helpers for raw-byte analysis and recovery.

Historically this helper read PAL bodies and ranked candidates using PAL
match status. Those inputs have been removed. Every queue decision, snapshot,
and gate now uses independently measured target records. PROVENANCE.md governs
the evidence required before accepting an implementation or name.

``queue``
    Rank target measurements by missing bytes and residual class. No external
    source corpus, symbol database, or cross-build match status is consumed.

``snapshot --source TU --output FILE``
    Audit every ported function in one TU into a temporary directory and save
    ``{function: aligned bytes}``.  Never writes artifacts or the summary.

``gate --source TU --baseline FILE [--target FN ...] [--neutral]``
    Re-audit the TU and compare with the snapshot.  Passes when no function
    loses matching bytes or accuracy and no function stops compiling, and
    (unless ``--neutral``) every target strictly gains accuracy.  ``--neutral``
    is the gate for readability-only edits: nothing may move down.
"""

import argparse
import hashlib
import json
import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "verify"))

import raw_xbe_structural as raw  # noqa: E402

RECORDS = ROOT / "artifacts" / "raw_xbe_structural"
CACHE = ROOT / "artifacts" / "bytematch" / "record_cache.json"
ATTEMPTED = ROOT / "artifacts" / "byte_campaign" / "attempted.json"
CAMPAIGNS = ROOT / "artifacts" / "byte_campaign" / "campaigns.jsonl"
# Retry ordinary failed shape attempts after two completed runs. Evidence-backed
# ceilings remain parked until their classification is explicitly changed.
RECENT_SKIP_RESULTS = frozenset(("no_gain", "reverted"))
SKIP_RESULTS = RECENT_SKIP_RESULTS | frozenset(("behavior_risk", "regarg_ceiling"))
DEPENDENCY_PATHS = (ROOT / "src" / "types.h", ROOT / "build" / "generated" / "decl.h")


# ---------------------------------------------------------------- records

def aligned(record):
    """``(matching, compared, accuracy)`` for one record, or None."""
    if record.get("verdict") == "structural exact":
        count = record.get("matching_non_relocation_bytes") or 0
        return count, count, 1.0
    block = record.get("aligned_byte_match") or {}
    if record.get("verdict") != "structural differ" or block.get("status") != "scored":
        return None
    if block.get("byte_accuracy") is None:
        return None
    return block["matching_bytes"], block["compared_bytes"], block["byte_accuracy"]


def _slim(record):
    """The fields the queue needs; full records are ~10KB each."""
    block = record.get("aligned_byte_match") or {}
    return {"function": record.get("function"), "address": record.get("address"),
            "lane": record.get("lane"), "generated_at": record.get("generated_at", ""),
            "verdict": record.get("verdict"), "source": record.get("source"),
            "register_argument_residual": record.get("register_argument_residual"),
            "matching_non_relocation_bytes": record.get("matching_non_relocation_bytes"),
            "aligned_byte_match": {key: block.get(key) for key in (
                "status", "byte_accuracy", "byte_accuracy_upper_bound",
                "matching_bytes", "compared_bytes", "uncertain_relocation_bytes",
                "mismatched_relocations", "candidate_only_instructions",
                "reference_only_instructions", "difference_classes")}}


def load_records():
    """Latest record per address, through a stat-keyed cache.

    Parsing every record over the /mnt/g mount costs ~80s; a stat scan
    costs ~5s, so only new or changed records are parsed.
    """
    try:
        cache = json.loads(CACHE.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        cache = {}
    fresh, latest = {}, {}
    for entry in os.scandir(RECORDS):
        if not entry.name.endswith(".json") or entry.name == "summary.json":
            continue
        stat = entry.stat()
        key = "%d:%d" % (stat.st_mtime_ns, stat.st_size)
        cached = cache.get(entry.name)
        if cached and cached[0] == key:
            record = cached[1]
        else:
            try:
                record = _slim(json.loads(Path(entry.path).read_text(encoding="utf-8")))
            except (OSError, ValueError):
                continue
        fresh[entry.name] = [key, record]
        address = record.get("address")
        if not address or record.get("lane") != "raw_xbe_structural":
            continue
        current = latest.get(address)
        if current is None or record["generated_at"] > current["generated_at"]:
            latest[address] = record
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    CACHE.write_text(json.dumps(fresh), encoding="utf-8")
    return latest


def relative_source(record):
    path = (record.get("source") or {}).get("path")
    if not path:
        return None
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path


# ---------------------------------------------------------------- commands

def dependency_fingerprint(source):
    """Digest the TU and generated declarations/types that constrain codegen."""
    digest = hashlib.sha256()
    for path in (ROOT / source,) + DEPENDENCY_PATHS:
        digest.update(str(path.relative_to(ROOT)).encode("utf-8"))
        try:
            digest.update(path.read_bytes())
        except OSError:
            digest.update(b"<missing>")
    return digest.hexdigest()


def recent_campaign_runs(limit=2):
    """Names of the newest completed campaign runs."""
    try:
        lines = CAMPAIGNS.read_text(encoding="utf-8").splitlines()
    except OSError:
        return frozenset()
    runs = []
    for line in reversed(lines):
        try:
            run = json.loads(line).get("run")
        except ValueError:
            continue
        if run and run not in runs:
            runs.append(run)
        if len(runs) == limit:
            break
    return frozenset(runs)


def skip_attempt(attempt, recent_runs, fingerprint=None):
    """Whether a prior result should suppress this queue entry."""
    result = attempt.get("result")
    if result not in SKIP_RESULTS:
        return False
    recorded_fingerprint = attempt.get("dependency_fingerprint")
    if recorded_fingerprint and fingerprint != recorded_fingerprint:
        return False
    if result in RECENT_SKIP_RESULTS:
        run = attempt.get("run")
        return run in recent_runs if run else False
    return True


def cmd_fingerprint(args):
    """Print the dependency fingerprint stored with an attempt result."""
    value = dependency_fingerprint(args.source)
    if args.json:
        print(json.dumps({"source": args.source, "dependency_fingerprint": value}, indent=1))
    else:
        print(value)
    return 0


def cmd_metrics(args):
    """Print the raw-byte metrics used to judge campaign progress."""
    summary_path = RECORDS / "summary.json"
    try:
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print("cannot read %s: %s" % (summary_path, exc), file=sys.stderr)
        return 1
    aligned_totals = summary.get("aligned_byte_totals") or {}
    function_totals = summary.get("function_totals") or {}
    metrics = {
        "generated_at": summary.get("generated_at"),
        "audited_functions": summary.get("audited_function_count", 0),
        "comparable_functions": (function_totals.get("structural_exact", 0) +
                                 function_totals.get("structural_differ", 0)),
        "structural_exact_functions": function_totals.get("structural_exact", 0),
        "structural_differ_functions": function_totals.get("structural_differ", 0),
        "exact_function_coverage": summary.get("exact_among_comparable"),
        "exact_byte_coverage": summary.get("exact_byte_coverage_of_comparable"),
        "aligned_byte_accuracy_lower": aligned_totals.get("byte_accuracy_lower"),
        "aligned_byte_accuracy_upper": aligned_totals.get("byte_accuracy_upper"),
    }
    if args.json:
        print(json.dumps(metrics, indent=1))
    else:
        print("Raw-XBE exact coverage")
        print("  exact functions: %d/%d (%.2f%%)" % (
            metrics["structural_exact_functions"], metrics["comparable_functions"],
            100.0 * (metrics["exact_function_coverage"] or 0.0)))
        print("  exact bytes:     %.2f%% of comparable bytes" %
              (100.0 * (metrics["exact_byte_coverage"] or 0.0)))
        print("  aligned bytes:   %.2f%%..%.2f%%" % (
            100.0 * (metrics["aligned_byte_accuracy_lower"] or 0.0),
            100.0 * (metrics["aligned_byte_accuracy_upper"] or 0.0)))
    return 0


def classify_residual(record):
    """Choose the cheapest evidence lane for a non-exact function."""
    block = record.get("aligned_byte_match") or {}
    classes = block.get("difference_classes") or {}
    abi = record.get("register_argument_residual") or {}
    if abi.get("status") == "exact_except_register_abi":
        return "abi", 0.05
    if (block.get("uncertain_relocation_bytes", 0) and
            not block.get("mismatched_relocations") and
            not classes):
        return "relocation", 0.0
    candidate_only = sum(count for kind, count in classes.items()
                         if kind.startswith("candidate_only:"))
    reference_only = sum(count for kind, count in classes.items()
                         if kind.startswith("reference_only:"))
    structural = candidate_only + reference_only
    shape = sum(classes.get(kind, 0) for kind in
                ("operand_order", "immediate", "encoding_size", "encoding"))
    type_like = sum(classes.get(kind, 0) for kind in
                    ("operands", "displacement", "stack_offset"))
    register = classes.get("register", 0) + sum(
        count for kind, count in classes.items() if kind.endswith(":register_save"))
    total = sum(classes.values()) or 1
    if structural > max(shape + type_like + register, total // 3):
        return "relift", 0.15
    if type_like > shape + register:
        return "types", 0.30
    if register > shape and register >= total // 3:
        return "frame", 0.20
    return "shape", 0.65


def queue_priority(missing, lane, probability, accuracy):
    """Expected byte gain per unit effort; deterministic lanes rank first."""
    lane_cost = {"shape": 1.0, "types": 4.0, "frame": 5.0, "relift": 8.0,
                 "abi": 10.0, "relocation": 100.0}
    exact_bonus = 1.5 if accuracy >= 0.95 else 1.0
    return (missing * probability * exact_bonus /
            lane_cost[lane])


def cmd_queue(args):
    try:
        attempted = json.loads(ATTEMPTED.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        attempted = {}
    recent_runs = recent_campaign_runs()
    rows = []
    for record in load_records().values():
        measured = aligned(record)
        if measured is None:
            continue
        matching, compared, accuracy = measured
        if accuracy >= args.max_accuracy or accuracy < args.min_accuracy:
            continue
        source = relative_source(record)
        if not source:
            continue
        if args.tu and source not in args.tu:
            continue
        fingerprint = dependency_fingerprint(source)
        if skip_attempt(attempted.get(record["function"], {}), recent_runs,
                        fingerprint):
            continue
        missing = compared - matching
        lane, probability = classify_residual(record)
        score = queue_priority(missing, lane, probability, accuracy)
        rows.append({"function": record["function"], "address": record["address"],
                     "source": source, "accuracy": round(accuracy, 4),
                     "missing_bytes": missing, "lane": lane,
                     "fix_probability": probability, "score": round(score, 2),
                     "causes": raw_causes(record)})
    rows.sort(key=lambda row: (-row["score"], -row["missing_bytes"]))
    selected = rows
    if args.lane:
        selected = [row for row in rows if row["lane"] in args.lane]
    by_tu = {}
    for row in selected:
        by_tu.setdefault(row["source"], []).append(row)
    tus = sorted(by_tu.items(), key=lambda item: -sum(row["score"] for row in item[1]))
    if args.max_tus:
        tus = tus[:args.max_tus]
    plan = [{"source": source, "score": round(sum(row["score"] for row in items), 1),
             "targets": items[:args.per_tu]} for source, items in tus]
    if args.json:
        Path(args.json).write_text(json.dumps(plan, indent=1) + "\n", encoding="utf-8")
    total = sum(len(tu["targets"]) for tu in plan)
    lane_counts = {}
    for row in rows:
        lane_counts[row["lane"]] = lane_counts.get(row["lane"], 0) + 1
    print("%d TUs, %d targets (of %d target candidates; %s)" % (
        len(plan), total, len(rows),
        ", ".join("%s=%d" % item for item in sorted(lane_counts.items()))))
    for tu in plan:
        print("%7.1f  %s" % (tu["score"], tu["source"]))
        for row in tu["targets"]:
            print("         %-36s %5.1f%%  -%-5d %-8s %s" % (
                row["function"], 100 * row["accuracy"], row["missing_bytes"],
                row["lane"],
                ",".join(sorted(row["causes"]))[:40]))
    return 0


def raw_causes(record):
    classes = (record.get("aligned_byte_match") or {}).get("difference_classes") or {}
    return {kind: count for kind, count in classes.items() if kind != "branch_target"} or classes


def measure_tu(source):
    """``{function: [matching, compared, accuracy] | None}`` for one TU."""
    source_path = (ROOT / source).resolve()
    items = raw._eligible_functions(Path(source).as_posix())
    if not items:
        raise SystemExit("no ported functions found for %s" % source)
    # Parallel workers share build/generated/decl.h.  Regenerate only when
    # kb.json is newer, so concurrent gates never rewrite it under each other.
    decl = raw.vc71.DECL_H
    stale = not decl.is_file() or decl.stat().st_mtime < (ROOT / "kb.json").stat().st_mtime
    if stale and not raw.vc71.regen_decl_header(quiet=True):
        raise SystemExit("decl.h regeneration failed")
    with tempfile.TemporaryDirectory(prefix="target_campaign_") as scratch:
        results = raw._audit_tu(source_path, items, Path(scratch), raw._decl_hash())
    return {item["function"]: (list(aligned(record)) if aligned(record) else None)
            for item, record in results}


def cmd_snapshot(args):
    scores = measure_tu(args.source)
    Path(args.output).write_text(json.dumps(scores, indent=1) + "\n", encoding="utf-8")
    measured = [value for value in scores.values() if value]
    print("%s: %d functions, %d/%d aligned bytes" % (
        args.source, len(scores), sum(v[0] for v in measured), sum(v[1] for v in measured)))
    return 0


def cmd_gate(args):
    before = json.loads(Path(args.baseline).read_text(encoding="utf-8"))
    after = measure_tu(args.source)
    failures, gains = [], []
    for function, old in before.items():
        new = after.get(function)
        if old and not new:
            failures.append("%s: no longer measurable (compile/audit failure)" % function)
            continue
        if not old or not new:
            continue
        if new[0] < old[0] or new[2] < old[2] - 1e-9:
            failures.append("%s: %d->%d bytes, %.2f%%->%.2f%%" % (
                function, old[0], new[0], 100 * old[2], 100 * new[2]))
        elif new[2] > old[2] + 1e-9:
            gains.append("%s: %.2f%%->%.2f%% (+%d bytes)" % (
                function, 100 * old[2], 100 * new[2], new[0] - old[0]))
    if not args.neutral:
        for target in args.target or []:
            old, new = before.get(target), after.get(target)
            if not (old and new and new[2] > old[2] + 1e-9):
                failures.append("%s: target did not gain" % target)
    for line in gains:
        print("GAIN  " + line)
    for line in failures:
        print("FAIL  " + line)
    print("PASS" if not failures else "REJECT")
    return 0 if not failures else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    q = sub.add_parser("queue")
    q.add_argument("--min-accuracy", type=float, default=0.75,
                   help="below this a gap is usually a lift bug, not shape")
    q.add_argument("--max-accuracy", type=float, default=1.0)
    q.add_argument("--tu", action="append", help="restrict to src/... path; repeatable")
    q.add_argument("--max-tus", type=int, default=6)
    q.add_argument("--per-tu", type=int, default=8)
    q.add_argument("--lane", action="append",
                   choices=("shape", "types", "frame", "relift", "abi", "relocation"),
                   help="restrict to a classified residual lane; repeatable")
    q.add_argument("--json")
    m = sub.add_parser("metrics")
    m.add_argument("--json", action="store_true")
    f = sub.add_parser("fingerprint")
    f.add_argument("--source", required=True)
    f.add_argument("--json", action="store_true")
    s = sub.add_parser("snapshot")
    s.add_argument("--source", required=True)
    s.add_argument("--output", required=True)
    g = sub.add_parser("gate")
    g.add_argument("--source", required=True)
    g.add_argument("--baseline", required=True)
    g.add_argument("--target", action="append")
    g.add_argument("--neutral", action="store_true")
    args = parser.parse_args(argv)
    return {"queue": cmd_queue, "metrics": cmd_metrics,
            "fingerprint": cmd_fingerprint,
            "snapshot": cmd_snapshot, "gate": cmd_gate}[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
