#!/usr/bin/env python3
"""Raw-byte waiver for VC71 mnemonic floor drops.

    raw_waiver.py --source src/.../x.c [...] [--json OUT]

VC71 mnemonic % is a coarse proxy. Raw aligned-byte accuracy against the 2276
XBE is the stricter measure. When the two disagree, raw wins, but only
narrowly. A function is **waivable** when:

* its aligned matching bytes in the staged (index) source are strictly
  greater than at HEAD, and
* no function in the same TU loses a single aligned byte.

Both sides are measured here, from ``git show HEAD:<tu>`` and
``git show :<tu>``. Nothing is taken from a caller's claim. Each side is
compiled from a hidden sibling copy, so relative includes resolve and the
working file is never touched. Per-function optimization flags come from the
real path.

The pre-commit hook passes the waivable set to
``vc71_regression.py check --waive`` and ``update --lower``. Only those
functions' floors can drop.

Exit status: 0 when the report was produced, 1 on measurement failure.
"""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "verify"))
sys.path.insert(0, str(ROOT / "tools" / "bytematch"))

import raw_xbe_structural as raw  # noqa: E402
from pal_campaign import aligned  # noqa: E402


def _git_blob(spec):
    proc = subprocess.run(["git", "show", spec], cwd=ROOT, capture_output=True)
    return proc.stdout if proc.returncode == 0 else None


def _measure(real_rel, content, scratch):
    """``{function: (matching, compared, accuracy) | None}`` for one blob."""
    real = ROOT / real_rel
    items = raw._eligible_functions(real_rel)
    copy = real.parent / (".raw_waiver.%s" % real.name)
    copy.write_bytes(content)
    try:
        results = {}
        by_opt = {}
        for item in items:
            by_opt.setdefault(raw._function_opt(real, item["function"]), []).append(item)
        for opt, opt_items in sorted(by_opt.items()):
            candidate = raw._candidate_object(scratch, copy, opt)
            if not raw.vc71.compile_vc71(copy, candidate, opt=opt):
                raise RuntimeError("VC71 compile failed for %s at %s" % (real_rel, opt))
            for item in opt_items:
                try:
                    record = raw.audit(candidate, item["function"], item["address"], copy)
                except (OSError, raw.strict.NotComparable):
                    record = {}
                results[item["function"]] = aligned(record)
        return results
    finally:
        copy.unlink(missing_ok=True)


def evaluate(sources):
    report = {}
    with tempfile.TemporaryDirectory(prefix="raw_waiver_") as tmp:
        for rel in sources:
            rel = Path(rel).as_posix()
            head, staged = _git_blob("HEAD:%s" % rel), _git_blob(":%s" % rel)
            if head is None or staged is None:
                report[rel] = {"skipped": "not in HEAD or index"}
                continue
            before = _measure(rel, head, Path(tmp) / "head")
            after = _measure(rel, staged, Path(tmp) / "staged")
            lost, gained, rows = [], [], {}
            for fn in sorted(set(before) | set(after)):
                old, new = before.get(fn), after.get(fn)
                rows[fn] = {"before": old, "after": new}
                if old and (not new or new[0] < old[0]):
                    lost.append(fn)
                elif new and (not old or new[0] > old[0]):
                    gained.append(fn)
            report[rel] = {"functions": rows, "lost": lost, "gained": gained,
                           "waivable": [] if lost else gained}
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", nargs="+", required=True)
    parser.add_argument("--json")
    args = parser.parse_args(argv)
    decl = raw.vc71.DECL_H
    if (not decl.is_file() or decl.stat().st_mtime < (ROOT / "kb.json").stat().st_mtime) \
            and not raw.vc71.regen_decl_header(quiet=True):
        print("raw_waiver: decl.h regeneration failed")
        return 1
    try:
        report = evaluate(args.source)
    except RuntimeError as exc:
        print("raw_waiver: %s" % exc)
        return 1
    for rel, row in report.items():
        if "skipped" in row:
            print("%s: skipped (%s)" % (rel, row["skipped"]))
            continue
        print("%s: gained %d, lost %d, waivable %s" % (
            rel, len(row["gained"]), len(row["lost"]), ",".join(row["waivable"]) or "-"))
        for fn in row["lost"]:
            print("  lost   %s %s -> %s" % (fn, row["functions"][fn]["before"],
                                            row["functions"][fn]["after"]))
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
