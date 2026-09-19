#!/usr/bin/env python3
"""Build a reproducible summary of VC71 floor scores and score contexts."""

import argparse
import hashlib
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_FLOOR = ROOT / "tools/verify/vc71_scores.json"
DEFAULT_CONTEXTS = ROOT / "artifacts/score_context"
CEILING_RULES = frozenset(("regarg_structural_ceiling",
                           "regarg_static_helper_ceiling"))
EXCLUDED_RULES = CEILING_RULES | frozenset(("forwarding_reference",))


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _aggregate_hash(paths):
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda item: item.name):
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(bytes.fromhex(_sha256(path)))
    return digest.hexdigest()


def _display_path(path):
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def _band(score):
    if score == 100:
        return "100"
    floor = int(score // 10) * 10
    return "%d-%d" % (floor, floor + 9)


def _has_rule(context, rule):
    return any(item.get("rule") == rule
               for item in context.get("classification", []))


def _load_contexts(context_dir):
    context_paths = sorted(context_dir.glob("*.json"))
    contexts = {}
    invalid = []
    for path in context_paths:
        try:
            context = json.loads(path.read_text())
            name = context.get("name")
            if not name:
                raise ValueError("missing name")
            contexts[name] = context
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            invalid.append({"file": path.name, "error": str(exc)})
    return context_paths, contexts, invalid


def build_census(floor_path, context_dir, low_threshold=70.0,
                  gap_threshold=10.0, classifier_gap_threshold=12.0):
    floor_doc = json.loads(floor_path.read_text())
    floor = floor_doc["scores"]
    context_paths, contexts, invalid = _load_contexts(context_dir)

    joined_names = sorted(set(floor) & set(contexts))
    low_names = sorted(name for name, row in floor.items()
                       if row["score"] < low_threshold)
    low_contexts = [contexts[name] for name in low_names if name in contexts]

    bands = {}
    for row in floor.values():
        key = _band(row["score"])
        bands[key] = bands.get(key, 0) + 1

    context_schemas = {}
    for context in contexts.values():
        schema = str(context.get("schema", 1))
        context_schemas[schema] = context_schemas.get(schema, 0) + 1

    rule_counts = {}
    with_rule = 0
    multi_rule = 0
    for context in low_contexts:
        rules = context.get("classification", [])
        if rules:
            with_rule += 1
        if len(rules) > 1:
            multi_rule += 1
        for item in rules:
            rule = item.get("rule")
            if rule:
                rule_counts[rule] = rule_counts.get(rule, 0) + 1

    gaps = []
    classifier_gaps = []
    for name in joined_names:
        scores = contexts[name].get("scores", {})
        official = scores.get("official_pct")
        dp = scores.get("dp_lcs_pct")
        if official is None or dp is None:
            continue
        gap = dp - official
        if gap > gap_threshold:
            gaps.append((name, gap, floor[name]["score"] < low_threshold))
        if gap > classifier_gap_threshold:
            classifier_gaps.append(
                (name, gap, floor[name]["score"] < low_threshold))

    one_insn = [(name, row) for name, row in floor.items()
                 if row.get("n_r") == 1]
    regarg_scores = [floor[name]["score"] for name in joined_names
                     if _has_rule(contexts[name], "regarg_structural_ceiling")]

    return {
        "schema": 1,
        "inputs": {
            "floor": _display_path(floor_path),
            "floor_sha256": _sha256(floor_path),
            "floor_version": floor_doc.get("version"),
            "context_dir": _display_path(context_dir),
            "context_files": len(context_paths),
            "context_aggregate_sha256": _aggregate_hash(context_paths),
            "context_schema_counts": dict(sorted(context_schemas.items())),
            "invalid_contexts": invalid,
        },
        "join": {
            "floor_rows": len(floor),
            "context_names": len(contexts),
            "joined_rows": len(joined_names),
            "missing_context": sorted(set(floor) - set(contexts)),
            "stale_context": sorted(set(contexts) - set(floor)),
        },
        "distribution": dict(sorted(bands.items(), reverse=True)),
        "low_score": {
            "threshold": low_threshold,
            "count": len(low_names),
            "contexts": len(low_contexts),
            "with_any_rule": with_rule,
            "without_rule": len(low_contexts) - with_rule,
            "with_multiple_rules": multi_rule,
            "rule_hits": sum(rule_counts.values()),
            "rule_counts": dict(sorted(rule_counts.items())),
        },
        "metric_gap": {
            "threshold_pp": gap_threshold,
            "count": len(gaps),
            "low_score_count": sum(1 for _, _, low in gaps if low),
            "sum_pp": sum(gap for _, gap, _ in gaps),
            "classifier_threshold_pp": classifier_gap_threshold,
            "classifier_count": len(classifier_gaps),
            "classifier_low_score_count": sum(
                1 for _, _, low in classifier_gaps if low),
        },
        "one_instruction_references": {
            "count": len(one_insn),
            "perfect": sum(1 for _, row in one_insn if row["score"] == 100),
            "below_low_threshold": sum(
                1 for _, row in one_insn if row["score"] < low_threshold),
            "exact_zero": sum(1 for _, row in one_insn if row["score"] == 0),
            "low_rows": [
                {"name": name, "score": row["score"], "kind": row.get("kind")}
                for name, row in sorted(one_insn)
                if row["score"] < low_threshold
            ],
        },
        "regarg_structural_ceiling": {
            "count": len(regarg_scores),
            "below_70": sum(1 for score in regarg_scores if score < 70),
            "from_70_to_89": sum(
                1 for score in regarg_scores if 70 <= score < 90),
            "at_least_90": sum(1 for score in regarg_scores if score >= 90),
        },
    }


def build_candidates(floor_path, context_dir, min_score=50.0,
                     max_score=85.0, min_reference_instructions=6,
                     include_ceilings=False):
    """Return score-work candidates from stored verifier output, without compiling."""
    floor_doc = json.loads(floor_path.read_text())
    floor = floor_doc["scores"]
    _, contexts, _ = _load_contexts(context_dir)
    candidates = []
    skipped = {}

    for name, row in floor.items():
        score = row["score"]
        if score < min_score or score >= max_score:
            skipped["score_range"] = skipped.get("score_range", 0) + 1
            continue
        if row.get("kind") != "auto":
            skipped["non_function_reference"] = (
                skipped.get("non_function_reference", 0) + 1)
            continue
        if row.get("n_r", 0) < min_reference_instructions:
            skipped["short_reference"] = skipped.get("short_reference", 0) + 1
            continue

        context = contexts.get(name)
        if context is None:
            skipped["missing_context"] = skipped.get("missing_context", 0) + 1
            continue
        rules = sorted(item["rule"] for item in context.get("classification", [])
                       if item.get("rule"))
        excluded = sorted(set(rules) & EXCLUDED_RULES)
        if excluded and not include_ceilings:
            reason = "forwarding_reference" if "forwarding_reference" in excluded \
                else "structural_ceiling"
            skipped[reason] = skipped.get(reason, 0) + 1
            continue

        context_scores = context.get("scores", {})
        official = context_scores.get("official_pct", score)
        dp_lcs = context_scores.get("dp_lcs_pct")
        candidates.append({
            "name": name,
            "source": row.get("source", ""),
            "score": score,
            "reference_instructions": row.get("n_r"),
            "candidate_instructions": context_scores.get(
                "n_cand_insns", row.get("n_c")),
            "dp_lcs_pct": dp_lcs,
            "metric_gap_pp": (round(dp_lcs - official, 1)
                              if dp_lcs is not None else None),
            "rules": rules,
        })

    candidates.sort(key=lambda row: (
        not bool(row["rules"]), -row["score"], row["name"]))
    return {
        "schema": 1,
        "inputs": {
            "floor": _display_path(floor_path),
            "floor_sha256": _sha256(floor_path),
            "context_dir": _display_path(context_dir),
        },
        "filters": {
            "min_score": min_score,
            "max_score": max_score,
            "min_reference_instructions": min_reference_instructions,
            "include_ceilings": include_ceilings,
        },
        "count": len(candidates),
        "skipped": dict(sorted(skipped.items())),
        "candidates": candidates,
    }


def _candidate_tsv(report):
    lines = ["name\tsource\tscore\tn_r\tn_c\tdp_lcs\tgap_pp\trules"]
    for row in report["candidates"]:
        lines.append("\t".join((
            row["name"], row["source"], "%.1f" % row["score"],
            str(row["reference_instructions"] or ""),
            str(row["candidate_instructions"] or ""),
            ("%.1f" % row["dp_lcs_pct"] if row["dp_lcs_pct"] is not None
             else ""),
            ("%.1f" % row["metric_gap_pp"] if row["metric_gap_pp"] is not None
             else ""),
            ",".join(row["rules"]),
        )))
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--floor", type=Path, default=DEFAULT_FLOOR)
    parser.add_argument("--contexts", type=Path, default=DEFAULT_CONTEXTS)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--check", action="store_true",
                        help="fail if --output differs from generated report")
    parser.add_argument("--candidates", action="store_true",
                        help="emit score-work candidates from stored score data")
    parser.add_argument("--min-score", type=float, default=50.0)
    parser.add_argument("--max-score", type=float, default=85.0)
    parser.add_argument("--min-reference-instructions", type=int, default=6)
    parser.add_argument("--include-ceilings", action="store_true",
                        help="include known structural-ceiling candidates")
    parser.add_argument("--format", choices=("json", "tsv"), default="json",
                        help="candidate output format (default: json)")
    args = parser.parse_args()
    if args.min_score >= args.max_score:
        parser.error("--min-score must be less than --max-score")
    if args.min_reference_instructions < 1:
        parser.error("--min-reference-instructions must be positive")
    if args.candidates:
        report = build_candidates(
            args.floor.resolve(), args.contexts.resolve(), args.min_score,
            args.max_score, args.min_reference_instructions,
            args.include_ceilings)
        text = (_candidate_tsv(report) if args.format == "tsv"
                else json.dumps(report, indent=2, sort_keys=True) + "\n")
    else:
        report = build_census(args.floor.resolve(), args.contexts.resolve())
        text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.check:
        if not args.output:
            parser.error("--check requires --output")
        current = args.output.read_text() if args.output.exists() else ""
        if current != text:
            print("VC71 census manifest is stale: %s" % args.output,
                  file=sys.stderr)
            return 1
        print("VC71 census manifest is current: %s" % args.output)
        return 0
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
