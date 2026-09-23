#!/usr/bin/env python3
"""List ported functions under a VC71 score threshold, worst first.

A zero-pre-work picker for score work.  Scores are merged per function the
same way the dashboard does it (vc71_current.json layered over the committed
floor vc71_scores.json), keyed by address so stale floor names do not produce
duplicates, and restricted to kb.json entries with ported == true.

Each row carries the score-context rules (artifacts/score_context/<name>.json)
so known structural ceilings can be skipped or grouped.

Examples:
  low_scores.py                         # all ported < 90%, worst first, by band
  low_scores.py --group file            # per source file, worst file first
  low_scores.py --group rule            # per score-context rule (lever family)
  low_scores.py --no-ceilings --limit 20
  low_scores.py --min-size 20 --format tsv
"""

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
KB = ROOT / "kb.json"
CURRENT = ROOT / "tools/verify/vc71_current.json"
FLOOR = ROOT / "tools/verify/vc71_scores.json"
CONTEXTS = ROOT / "artifacts/score_context"
CEILING_RULES = frozenset(("regarg_structural_ceiling",
                           "regarg_static_helper_ceiling"))


def _load_scores(path):
    try:
        return json.loads(path.read_text()).get("scores") or {}
    except (OSError, ValueError):
        return {}


def _norm_addr(addr):
    return "0x%x" % int(addr, 16)


def _rules(name):
    try:
        ctx = json.loads((CONTEXTS / (name + ".json")).read_text())
    except (OSError, ValueError):
        return []
    return sorted({c.get("rule") for c in ctx.get("classification", [])
                   if c.get("rule")})


def collect(max_score, min_score, min_size):
    kb = json.loads(KB.read_text())
    ported = {}
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            if fn.get("ported") is True and fn.get("addr"):
                ported[_norm_addr(fn["addr"])] = dict(fn, obj=obj.get("name"))

    # addr -> (entry, origin); current wins over floor per address.
    by_addr = {}
    for origin, scores in (("floor", _load_scores(FLOOR)),
                           ("current", _load_scores(CURRENT))):
        for score_name, entry in scores.items():
            if not isinstance(entry, dict) or "addr" not in entry:
                continue
            by_addr[_norm_addr(entry["addr"])] = (entry, origin, score_name)

    rows = []
    for addr, (entry, origin, score_name) in by_addr.items():
        kb_entry = ported.get(addr)
        score = entry.get("score")
        if kb_entry is None or score is None:
            continue
        if not (min_score <= score < max_score):
            continue
        if (entry.get("n_r") or 0) < min_size:
            continue
        name = kb_entry.get("name") or score_name
        rows.append({
            "score": score,
            "addr": addr,
            "name": name,
            "n_r": entry.get("n_r") or 0,
            "source": entry.get("source") or "?",
            "obj": kb_entry.get("obj") or "?",
            "origin": origin,
            "rules": _rules(name),
        })
    rows.sort(key=lambda r: (r["score"], -r["n_r"]))
    return rows


def _band(score):
    lo = int(score // 10) * 10
    return "%d-%d%%" % (lo, lo + 9)


def _fmt(r):
    return "  %5.1f  %-10s %-44s %5d  %-44s %s%s" % (
        r["score"], r["addr"], r["name"][:44], r["n_r"], r["source"][:44],
        ",".join(r["rules"]) or "-",
        "" if r["origin"] == "current" else "  [floor]")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--max-score", type=float, default=90.0,
                    help="exclusive upper bound (default 90)")
    ap.add_argument("--min-score", type=float, default=0.0)
    ap.add_argument("--min-size", type=int, default=0,
                    help="minimum reference instruction count (n_r)")
    ap.add_argument("--no-ceilings", action="store_true",
                    help="drop known @<reg> structural-ceiling functions")
    ap.add_argument("--group", choices=("band", "file", "obj", "rule", "none"),
                    default="band")
    ap.add_argument("--limit", type=int, default=0,
                    help="max rows per group (0 = all)")
    ap.add_argument("--format", choices=("text", "tsv", "json"), default="text")
    args = ap.parse_args()

    rows = collect(args.max_score, args.min_score, args.min_size)
    if args.no_ceilings:
        rows = [r for r in rows if not CEILING_RULES.intersection(r["rules"])]

    if args.format == "json":
        json.dump(rows[:args.limit] if args.limit else rows, sys.stdout, indent=1)
        print()
        return 0
    if args.format == "tsv":
        print("score\taddr\tname\tn_r\tsource\tobj\trules\torigin")
        for r in rows[:args.limit] if args.limit else rows:
            print("%.1f\t%s\t%s\t%d\t%s\t%s\t%s\t%s" % (
                r["score"], r["addr"], r["name"], r["n_r"], r["source"],
                r["obj"], ",".join(r["rules"]), r["origin"]))
        return 0

    groups = defaultdict(list)
    for r in rows:
        if args.group == "band":
            keys = [_band(r["score"])]
        elif args.group == "file":
            keys = [r["source"]]
        elif args.group == "obj":
            keys = [r["obj"]]
        elif args.group == "rule":
            keys = r["rules"] or ["(unclassified)"]
        else:
            keys = ["all"]
        for k in keys:
            groups[k].append(r)

    # Worst group first: lowest band, or the group whose worst member is lowest
    # (ties broken by group size, larger first).
    order = sorted(groups, key=lambda k: (min(r["score"] for r in groups[k]),
                                          -len(groups[k])))
    print("%d ported functions with %.1f <= score < %.1f%s" % (
        len(rows), args.min_score, args.max_score,
        " (ceilings excluded)" if args.no_ceilings else ""))
    for k in order:
        g = groups[k]
        avg = sum(r["score"] for r in g) / len(g)
        print("\n== %s  (%d fns, avg %.1f%%, %d ref insns)" % (
            k, len(g), avg, sum(r["n_r"] for r in g)))
        for r in g[:args.limit] if args.limit else g:
            print(_fmt(r))
        if args.limit and len(g) > args.limit:
            print("  ... %d more" % (len(g) - args.limit))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.stderr.close()
        sys.exit(0)
