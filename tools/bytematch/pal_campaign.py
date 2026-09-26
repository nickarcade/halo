#!/usr/bin/env python3
"""Fast deterministic helpers for the raw-byte campaign (/byte-campaign).

The campaign raises raw-XBE aligned byte accuracy using the punpckhdq/halo
decompilation of PAL build 2342 (``../halo-pal-2342``) as a shape hint.  The
LLM work is one worker per TU; everything that can be computed runs here so
workers never read whole files or re-derive state.

``queue``
    Join the latest raw-XBE structural records with the PAL codegraph index
    and PAL per-object status.  Rank by recoverable bytes, weighting PAL
    objects that are ``Matching`` (every function byte-exact against 2342).

``pal FUNCTION``
    Print the PAL definition of one function (file, line range, object status,
    body).  Advisory evidence only: 2342 is not 2276.

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
import json
import os
import sqlite3
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "verify"))

import raw_xbe_structural as raw  # noqa: E402

PAL_ROOT = ROOT.parent / "halo-pal-2342"
PAL_DB = PAL_ROOT / ".codegraph" / "codegraph.db"
PAL_CONFIG = PAL_ROOT / "config" / "config.json"
PROPOSALS = ROOT / "artifacts" / "punpckhdq_import" / "name_proposals.json"
RECORDS = ROOT / "artifacts" / "raw_xbe_structural"
CACHE = ROOT / "artifacts" / "bytematch" / "record_cache.json"
ATTEMPTED = ROOT / "artifacts" / "byte_campaign" / "attempted.json"
# Ledger results that mean "another shape attempt will not help".
SKIP_RESULTS = frozenset(("no_gain", "reverted", "behavior_risk", "regarg_ceiling"))
STATUS_WEIGHT = {"Matching": 1.0, "NonMatching": 0.4}


# ---------------------------------------------------------------- PAL index

def pal_object_status():
    """``{source path without extension: status}`` from the PAL config."""
    config = json.loads(PAL_CONFIG.read_text(encoding="utf-8"))
    status = {}
    for project in config.get("projects", []):
        for obj in project.get("objects", []):
            status[str(Path(obj["name"]).with_suffix(""))] = obj.get("status", "MISSING")
    return status


class PalIndex:
    def __init__(self):
        self.db = sqlite3.connect("file:%s?mode=ro" % PAL_DB, uri=True)
        self.status = pal_object_status()
        self.alias = {}
        if PROPOSALS.is_file():
            for proposal in json.loads(PROPOSALS.read_text(encoding="utf-8")):
                if proposal.get("confidence") == "high":
                    self.alias[proposal["our_name"]] = proposal["proposed_name"]

    def lookup(self, name):
        """Definitions of ``name`` in PAL .c/.cpp files (headers excluded)."""
        names = [name] + ([self.alias[name]] if name in self.alias else [])
        rows = []
        for candidate in names:
            rows = self.db.execute(
                "select file_path, start_line, end_line from nodes "
                "where kind = 'function' and name = ? and "
                "(file_path like '%.c' or file_path like '%.cpp')",
                (candidate,)).fetchall()
            if rows:
                break
        hits = []
        for file_path, start, end in rows:
            status = self.status.get(str(Path(file_path).with_suffix("")), "MISSING")
            hits.append({"file": file_path, "start": start, "end": end,
                         "lines": end - start + 1, "status": status,
                         "via_alias": candidate != name})
        return hits


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
            "matching_non_relocation_bytes": record.get("matching_non_relocation_bytes"),
            "aligned_byte_match": {key: block.get(key) for key in (
                "status", "byte_accuracy", "matching_bytes", "compared_bytes",
                "difference_classes")}}


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

def cmd_queue(args):
    index = PalIndex()
    try:
        attempted = json.loads(ATTEMPTED.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        attempted = {}
    rows = []
    for record in load_records().values():
        measured = aligned(record)
        if measured is None:
            continue
        if attempted.get(record["function"], {}).get("result") in SKIP_RESULTS:
            continue
        matching, compared, accuracy = measured
        if accuracy >= args.max_accuracy or accuracy < args.min_accuracy:
            continue
        source = relative_source(record)
        if args.tu and source not in args.tu:
            continue
        hits = index.lookup(record["function"])
        if len(hits) != 1:
            if not args.include_unmapped:
                continue
            hit = {"file": None, "status": "AMBIGUOUS" if hits else "UNMAPPED", "lines": 0}
        else:
            hit = hits[0]
        if args.matching_only and hit["status"] != "Matching":
            continue
        missing = compared - matching
        weight = STATUS_WEIGHT.get(hit["status"], 0.1)
        rows.append({"function": record["function"], "address": record["address"],
                     "source": source, "accuracy": round(accuracy, 4),
                     "missing_bytes": missing, "pal_file": hit["file"],
                     "pal_lines": [hit.get("start"), hit.get("end")],
                     "pal_status": hit["status"], "score": round(missing * weight, 1),
                     "causes": raw_causes(record)})
    rows.sort(key=lambda row: -row["score"])
    by_tu = {}
    for row in rows:
        by_tu.setdefault(row["source"], []).append(row)
    tus = sorted(by_tu.items(), key=lambda item: -sum(row["score"] for row in item[1]))
    if args.max_tus:
        tus = tus[:args.max_tus]
    plan = [{"source": source, "score": round(sum(row["score"] for row in items), 1),
             "targets": items[:args.per_tu]} for source, items in tus]
    if args.json:
        Path(args.json).write_text(json.dumps(plan, indent=1) + "\n", encoding="utf-8")
    total = sum(len(tu["targets"]) for tu in plan)
    print("%d TUs, %d targets (of %d mapped candidates)" % (len(plan), total, len(rows)))
    for tu in plan:
        print("%7.1f  %s" % (tu["score"], tu["source"]))
        for row in tu["targets"]:
            print("         %-40s %5.1f%%  -%-5d %-11s %s" % (
                row["function"], 100 * row["accuracy"], row["missing_bytes"],
                row["pal_status"], ",".join(sorted(row["causes"]))[:40]))
    return 0


def raw_causes(record):
    classes = (record.get("aligned_byte_match") or {}).get("difference_classes") or {}
    return {kind: count for kind, count in classes.items() if kind != "branch_target"} or classes


def cmd_pal(args):
    index = PalIndex()
    hits = index.lookup(args.function)
    if not hits:
        print("no PAL definition for %s" % args.function)
        return 1
    for hit in hits:
        print("// PAL 2342 %s:%d-%d  object status: %s%s  (ADVISORY: 2342 != 2276)" % (
            hit["file"], hit["start"], hit["end"], hit["status"],
            "  via name proposal" if hit["via_alias"] else ""))
        lines = (PAL_ROOT / hit["file"]).read_text(
            encoding="utf-8", errors="replace").splitlines()
        print("\n".join(lines[hit["start"] - 1:hit["end"]]))
    return 0


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
    with tempfile.TemporaryDirectory(prefix="pal_campaign_") as scratch:
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
    q.add_argument("--matching-only", action="store_true")
    q.add_argument("--include-unmapped", action="store_true")
    q.add_argument("--json")
    p = sub.add_parser("pal")
    p.add_argument("function")
    s = sub.add_parser("snapshot")
    s.add_argument("--source", required=True)
    s.add_argument("--output", required=True)
    g = sub.add_parser("gate")
    g.add_argument("--source", required=True)
    g.add_argument("--baseline", required=True)
    g.add_argument("--target", action="append")
    g.add_argument("--neutral", action="store_true")
    args = parser.parse_args(argv)
    return {"queue": cmd_queue, "pal": cmd_pal, "snapshot": cmd_snapshot,
            "gate": cmd_gate}[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
