#!/usr/bin/env python3
"""
check_function_placement.py — Cross-check the source file and kb.json object
a ported function lives in against the __FILE__ paths its original body
stamps.

The debug build's assert sites push a __FILE__ string such as
"c:\\halo\\SOURCE\\networking\\network_client_manager.c".  Such a string is
binary proof of the translation unit the function was compiled in.  When a
lift is filed in a different .c (or listed under a different kb.json object),
nothing in the build or VC71 scoring notices: the code still links and still
matches.  This audit reads the placement straight off the pristine XBE.

Method
------
For each `ported: true` kb.json function with a same-named definition in src/:

  1. Collect every string the original body [addr, function_bounds.json end)
     references (check_string_literals.referenced_strings).
  2. Keep the ones that look like source paths ("...\\source\\...\\X.c" or
     ".h", case-insensitive).  Only `.c` paths are evidence; `.h` paths come
     from header-resident inlines and legitimately differ from the TU.
  3. Compare the set of `.c` stems against:

  MISPLACED-FILE    the stem of the .c file that defines the function in src/
                    is not among the evidence stems.
  MISPLACED-OBJECT  the stem of the kb.json object the function is listed
                    under (foo.obj -> foo) is not among the evidence stems.
  OK                both match.
  UNKNOWN           no `.c` path string in the body (counted only; -v lists).

  A summary groups findings by (our file -> evidence stem) so systematic
  renames and intentional TU splits (one original .c spread over several of
  our files) stand out from one-off misplacements.

Known false positives: our repo sometimes splits one original TU across
several .c files or names a file differently from the original (every
function of such a file shows up under the same group); stale or short
function_bounds.json `end` (a body that runs into the next function can pick
up that function's path); and a path string reached through a table or
global rather than an immediate is invisible (UNKNOWN, not a finding).
Placement of functions without asserts is not checked at all.

Usage:
    python3 tools/audit/check_function_placement.py                 # findings + summary
    python3 tools/audit/check_function_placement.py -v              # + OK/UNKNOWN rows
    python3 tools/audit/check_function_placement.py --function model_get_marker_by_name
    python3 tools/audit/check_function_placement.py --files src/halo/math/random_math.c
    python3 tools/audit/check_function_placement.py --json <path>
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_arg_counts as cac  # noqa: E402  (kb loader)
import check_callee_identity as cci  # noqa: E402  (bounds, source walking)
import check_string_literals as csl  # noqa: E402  (referenced_strings)

# Anchored at the start: an operand that points at a float constant in .rdata
# reads as a "string" that runs on into the next __FILE__ path, which would
# otherwise credit an unrelated TU (seen as spurious action_vehicle.c /
# actor_type_flood.c evidence).
_PATH_RE = re.compile(rb"^[a-z]:\\halo\\source\\(?:[^\\\0]+\\)*([^\\\0]+)\.([ch])$",
                      re.IGNORECASE)


def _object_map() -> Dict[int, str]:
    """addr -> kb.json object name (objects[].functions[] and top-level obj)."""
    kb = json.loads(cac.KB_PATH.read_text())
    out: Dict[int, str] = {}
    for obj in kb.get("objects", []):
        oname = obj.get("name") or ""
        for fn in obj.get("functions", []) or []:
            try:
                out.setdefault(int(fn.get("addr", ""), 16), oname)
            except (TypeError, ValueError):
                continue
    for k, v in kb.items():
        if k.startswith("0x") and isinstance(v, dict) and v.get("obj"):
            try:
                out.setdefault(int(k, 16), v["obj"])
            except ValueError:
                continue
    return out


def path_stems(refs: Dict[int, bytes]) -> Tuple[Set[str], Set[str]]:
    """(.c stems, .h stems), lowercased, of the source-path strings in refs."""
    c: Set[str] = set()
    h: Set[str] = set()
    for s in refs.values():
        m = _PATH_RE.match(s)
        if not m:
            continue
        stem = m.group(1).decode("latin-1").lower()
        (c if m.group(2).lower() == b"c" else h).add(stem)
    return c, h


class Row(NamedTuple):
    status:    str        # MISPLACED-FILE / MISPLACED-OBJECT / MISPLACED-BOTH / OK / UNKNOWN
    function:  str
    addr:      str
    path:      str
    line:      int
    obj:       str
    evidence:  List[str]  # .c stems
    headers:   List[str]  # .h stems (informational)


def analyze(files: Optional[List[str]] = None,
            func_filter: Optional[str] = None) -> Tuple[List[Row], Dict]:
    entries = cac._load_kb()
    name_addrs: Dict[str, Set[int]] = {}
    ported: Dict[int, bool] = {}
    for e in entries:
        if e.name:
            name_addrs.setdefault(e.name, set()).add(e.addr)
        ported[e.addr] = e.ported
    objmap = _object_map()
    bounds = cci._load_bounds()

    funcs: Dict[str, cci.SourceFunc] = {}
    dups: Set[str] = set()
    for p in cci._collect_src_files(files, False, 0):
        if not p.is_file():
            continue
        for sf in cci.parse_source_file(p):
            if sf.name in funcs:
                dups.add(sf.name)
            else:
                funcs[sf.name] = sf

    stats = {"source_functions": len(funcs), "analyzed": 0, "no_kb_entry": 0,
             "not_ported": 0, "no_bounds": 0, "duplicate_names": len(dups),
             "ok": 0, "unknown": 0, "misplaced_file": 0, "misplaced_object": 0,
             "multi_stem": 0}
    rows: List[Row] = []

    for name, sf in sorted(funcs.items()):
        if name in dups:
            continue
        addrs = name_addrs.get(name)
        if not addrs:
            stats["no_kb_entry"] += 1
            continue
        addr = min(addrs)
        if func_filter is not None and func_filter not in (name, "0x%x" % addr):
            continue
        if not ported.get(addr):
            stats["not_ported"] += 1
            continue
        end = bounds.get(addr)
        if end is None or end <= addr:
            stats["no_bounds"] += 1
            continue
        refs, decoded = csl.referenced_strings(addr, end)
        if not decoded:
            stats["no_bounds"] += 1
            continue
        stats["analyzed"] += 1

        cstems, hstems = path_stems(refs)
        obj = objmap.get(addr, "")
        rel = cci._relpath(sf.path)
        if not cstems:
            status = "UNKNOWN"
            stats["unknown"] += 1
        else:
            if len(cstems) > 1:
                stats["multi_stem"] += 1
            bad_file = sf.path.stem.lower() not in cstems
            obj_stem = obj.rsplit(".", 1)[0].lower() if obj else ""
            bad_obj = bool(obj_stem) and obj_stem not in cstems
            stats["misplaced_file"] += bad_file
            stats["misplaced_object"] += bad_obj
            if bad_file and bad_obj:
                status = "MISPLACED-BOTH"
            elif bad_file:
                status = "MISPLACED-FILE"
            elif bad_obj:
                status = "MISPLACED-OBJECT"
            else:
                status = "OK"
                stats["ok"] += 1
        rows.append(Row(status, name, "0x%x" % addr, rel, sf.line, obj,
                        sorted(cstems), sorted(hstems)))

    return rows, stats


def report(rows: List[Row], stats: Dict, verbose: bool) -> None:
    bad = [r for r in rows if r.status.startswith("MISPLACED")]
    for r in sorted(rows, key=lambda r: (r.path, r.line)):
        if not r.status.startswith("MISPLACED") and not verbose:
            continue
        ev = ",".join(r.evidence) or "-"
        extra = "  (h: %s)" % ",".join(r.headers) if r.headers and verbose else ""
        print("%-16s %s:%d  %s@%s  obj=%s  evidence=%s%s"
              % (r.status, r.path, r.line, r.function, r.addr, r.obj or "-", ev, extra))

    groups: Dict[Tuple[str, str, str], List[Row]] = defaultdict(list)
    for r in bad:
        kind = "obj-only" if r.status == "MISPLACED-OBJECT" else "file"
        groups[(r.path, ",".join(r.evidence), kind)].append(r)
    if groups:
        # Files whose findings all point at one stem look like a rename/split.
        file_total = Counter(r.path for r in rows if r.status != "UNKNOWN")
        print("\nSummary by (our file -> evidence stem):")
        for (path, ev, kind), rs in sorted(groups.items(), key=lambda kv: (-len(kv[1]), kv[0])):
            print("  %4d/%-4d %-8s %s -> %s   e.g. %s@%s obj=%s"
                  % (len(rs), file_total[path], kind, path, ev, rs[0].function,
                     rs[0].addr, rs[0].obj or "-"))
    print("\n" + " ".join("%s=%d" % kv for kv in stats.items()))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--files", nargs="*")
    ap.add_argument("--function")
    ap.add_argument("--json")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    rows, stats = analyze(args.files, args.function)
    report(rows, stats, args.verbose)
    if args.json:
        Path(args.json).write_text(json.dumps(
            {"stats": stats, "rows": [r._asdict() for r in rows]}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
