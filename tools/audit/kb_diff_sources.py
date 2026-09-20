#!/usr/bin/env python3
"""Print the src/halo/*.c files whose kb.json-tracked functions changed
between two revisions.

The VC71 regression gate treats any kb.json edit as "could affect any TU" and
falls back to a full check --strict sweep across every baselined translation
unit (185+ files, 8-way parallel) -- appropriate for a real cross-cutting
change but not for the common case of a rename/ported-flag/decl edit
touching a handful of functions. This script narrows that: it diffs kb.json
structurally (by address, not by line) and reports only the source files that
own a changed, added, or removed function, so the gate can pass them to
`vc71_regression.py check --strict --source <files>` instead of a full sweep.

Usage: kb_diff_sources.py --base <git-rev> [--head <git-rev-or-omit-for-worktree>]
"""
import argparse
import json
import subprocess
import sys


def load(rev):
    if rev is None:
        with open("kb.json", encoding="utf-8") as f:
            return json.load(f)
    out = subprocess.run(["git", "show", f"{rev}:kb.json"],
                          capture_output=True, check=True)
    return json.loads(out.stdout)


def flatten(data):
    """addr -> (function_dict, owning_source_or_None)."""
    out = {}
    for obj in data.get("objects", []):
        obj_src = obj.get("source")
        for fn in obj.get("functions") or []:
            addr = fn.get("addr")
            if not addr:
                continue
            src = fn.get("source_path") or fn.get("file") or obj_src
            out[addr] = (fn, src)
    for key, value in data.items():
        if key in ("md5", "objects"):
            continue
        if isinstance(value, dict):
            addr = value.get("addr", key)
            out.setdefault(addr, (value, value.get("file")))
    return out


def to_repo_path(src):
    if not src:
        return None
    src = src.replace("\\", "/")
    return src if src.startswith("src/") else f"src/halo/{src}"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", required=True, help="git revision to diff from")
    ap.add_argument("--head", default=None,
                     help="git revision to diff to (default: working tree)")
    args = ap.parse_args()

    base = flatten(load(args.base))
    head = flatten(load(args.head))

    changed_sources = set()
    for addr in set(base) | set(head):
        b, h = base.get(addr), head.get(addr)
        if b == h:
            continue
        for entry in (b, h):
            if entry is None:
                continue
            path = to_repo_path(entry[1])
            if path:
                changed_sources.add(path)

    for path in sorted(changed_sources):
        print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
