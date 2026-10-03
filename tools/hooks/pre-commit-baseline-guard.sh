#!/usr/bin/env bash
# HALO-HOOK-TRIGGER: ^tools/kb_reg_baseline\.json$
# Pre-commit hook: protect the @<reg> entries in tools/kb_reg_baseline.json.
# New entries are allowed. An existing entry may change only in names or
# types: it must keep its parameter count and pin the same parameters to the
# same registers. Removing an entry or changing its registers is refused.

BASELINE="tools/kb_reg_baseline.json"

# Only run if the baseline is staged for commit.
. "$(dirname "${BASH_SOURCE[0]}")/lib-staged.sh"
if ! staged_list all | grep -qx "$BASELINE"; then
    exit 0
fi

# If the file is new in HEAD, nothing to protect.
git cat-file -e HEAD:"$BASELINE" 2>/dev/null || exit 0

# Compare each committed entry with its staged version.
python3 -c '
import json, subprocess, sys
sys.path.insert(0, "tools/audit")
from extract_reg_args import reg_shape
def load(rev):
    out = subprocess.run(["git", "show", rev + ":" + sys.argv[1]],
                         capture_output=True, check=True).stdout
    return json.loads(out).get("functions", {})
old, new = load("HEAD"), load("")
removed, changed = [], []
for addr in sorted(old, key=lambda a: int(a, 16)):
    decl = old[addr]
    if addr not in new:
        removed.append(f"  {addr}  {decl}")
    elif new[addr] != decl and reg_shape(new[addr]) != reg_shape(decl):
        changed += [f"  {addr}", f"    old: {decl}", f"    new: {new[addr]}"]
if removed or changed:
    print("ERROR: kb_reg_baseline.json entries are protected.")
    print()
    print("Existing @<reg> baseline entries cannot be removed, and an entry may")
    print("change only in names or types: same parameter count, same registers.")
    if removed:
        print("\nREMOVED entries:")
        print("\n".join(removed))
    if changed:
        print("\nCHANGED register shape:")
        print("\n".join(changed))
    print()
    print("If this is a deliberate policy change, use: git commit --no-verify")
    sys.exit(1)
' "$BASELINE"
