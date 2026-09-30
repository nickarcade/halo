#!/usr/bin/env bash
# HALO-HOOK-TRIGGER: \.c$
# Pre-commit readability gate (readable-lift Phase 3).
#
#   HARD  -- block if a staged .c file ADDS a raw function-pointer cast to a
#            literal address (((T(*)(A))0xADDR)(...)). These bypass kb.json and
#            the thunk system and hide calling-convention bugs; a raw cast is
#            never necessary (add the callee to kb.json instead).
#   HARD  -- block if a staged .c file ADDS a raw offset deref on a pointer that
#            came from an untyped cast of a call. The struct type was lost at
#            that producer's kb.json RETURN decl; type it there (a pointer return
#            is EAX either way, so it is codegen-neutral) and every caller gets
#            fields. Only ADDED lines gate, so the existing 7042 sites never
#            block an unrelated edit.
#   SOFT  -- print, but never block, the FUN_-call / offset-deref findings in
#            touched files (they legitimately grow as new code is lifted; the
#            ratchet in check_readability.py --check locks the wins over time).
#
# Bypass with --no-verify in an emergency.
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
. "$(dirname "${BASH_SOURCE[0]}")/lib-staged.sh"

if ! staged_list acmr | grep -q '\.c$'; then
    exit 0
fi

# One process runs all three checks (advisory staged findings, added raw
# function-pointer casts, added raw-offset derefs) -- see
# check_readability.py --pre-commit. It used to be two interpreter starts, two
# `git diff --cached` calls and a worktree-wide `git diff HEAD` (2.5 s of the 3.3 s).
python3 "$REPO_ROOT/tools/audit/check_readability.py" --pre-commit
exit $?
