#!/usr/bin/env bash
# HALO-HOOK-TRIGGER: ^(src/.*\.c|src/.*\.h|kb\.json|tools/equivalence/.*\.py)$
# Pre-commit: run Unicorn regression tests when source files are staged.
# Quick mode is 5 seeds per function, and the corpus has grown to 110 targets.
# Serially (--jobs 1, the runner's default) that is 426s measured -- not the
# "~130s" this comment used to claim, and not the "under 10 seconds" it claimed
# before that. The harness self-tests below add ~5s.
#
# Each target is an independent unicorn_diff subprocess, so the runner has
# always supported running them concurrently (ThreadPoolExecutor, a per-name
# lock so two entries sharing a name never race on the same smoke log, and the
# shared leaf cache disabled for jobs > 1 because it is selection metadata, not
# part of the verdict). The hook simply never asked for it. Measured on this
# 16-core box: 426s at -j1 vs 40s at -j8, with a byte-identical verdict line
# (83 passed, 2 failed, 0 errors, 25 skipped) and no target hitting the 120s
# per-target timeout.
#
# EVERY target is still checked on every run -- the gate is not scoped by guesswork
# about which files a regression can hide behind (a renamed callee, a changed
# struct offset in a shared header, a corrected kb.json decl all move targets in
# TUs the commit did not touch). What changed is that an unchanged target costs
# ~0 instead of ~3s:
#
#   * `--staged` compiles each target TU from a SNAPSHOT OF THE INDEX (never a
#     stale build/ object, never another agent's unstaged edits), with the exact
#     CMake Release flags and a decl.h regenerated from the staged kb.json.
#   * A target's PASS verdict is reused only when the content key of ALL its
#     inputs is unchanged: candidate object bytes (so any source/header/decl.h
#     change that moves codegen misses), the target row, the kb.json entries the
#     object references plus the function-address set, every tools/equivalence
#     *.py, function_bounds.json, the pristine XBE, versions and HALO_* env.
#     Only passes are cached; any doubt (unreadable cache, unpinnable input,
#     snapshot/compile failure) means the target runs, uncached.
#   * The full, uncached sweep stays available: regression_test.py --no-cache
#     (pre-push runs it), or plain regression_test.py (legacy working-tree run,
#     used by auto_reintegrate Gate 5 and CI).
#
# tools/equivalence/*.py is in the filter deliberately. It used to gate on
# src/kb.json only, so a commit that changed nothing but the harness itself --
# exactly the change that can break the harness -- skipped this hook entirely
# and reported no opinion at all (2026-07-29).

. "$(dirname "${BASH_SOURCE[0]}")/lib-staged.sh"
STAGED=$(staged_list all | grep -E '^(src/.*\.c|src/.*\.h|kb\.json|tools/equivalence/.*\.py)$')
if [ -z "$STAGED" ]; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
SCRIPT="$ROOT/tools/equivalence/regression_test.py"

if [ ! -f "$SCRIPT" ]; then
    exit 0
fi

# Find python with unicorn installed
VENV_PY="$ROOT/.venv/bin/python3"
if [ -x "$VENV_PY" ]; then
    PY="$VENV_PY"
else
    PY="python3"
fi

# Check unicorn is importable before running (skip silently if not installed)
if ! "$PY" -c "import unicorn" 2>/dev/null; then
    exit 0
fi

# Harness self-tests first, and only when the harness itself changed: they are
# the pins on the comparator's soundness (a wrong offset into the right global
# must still be REPORTED), and they fail in ~5s, so they should speak before
# the slower differential does.
HARNESS=$(echo "$STAGED" | grep '^tools/equivalence/.*\.py$')
SELFTESTS="$ROOT/tools/equivalence/run_all_tests.py"
if [ -n "$HARNESS" ] && [ -f "$SELFTESTS" ]; then
    echo "Running equivalence harness self-tests..."
    "$PY" "$SELFTESTS"
    RC=$?
    if [ $RC -ne 0 ]; then
        echo ""
        echo "Equivalence harness self-tests FAILED. Fix before committing."
        echo "Run: python3 tools/equivalence/run_all_tests.py -v"
        exit $RC
    fi
fi

# BATCH MODE -- defer the differential to the land gate, not away.
#
# HALO_BATCH_COMMIT=1 marks a commit from an automated batch (/auto-session ->
# /goal-lift, ~12 commits per batch). Those reach `main` only through
# tools/integrate/auto_reintegrate.py, whose Gate 5 runs this same suite once
# over the branch tip. The corpus is fixed, not scoped to the staged files, so
# per-commit runs re-derive the same verdict 12x at 40-48s each.
#
# Opt-in and fail-SAFE: unset (every interactive commit) runs the full suite as
# before; set on `main` is refused, because no land gate follows a commit made
# there. The self-tests above are never deferred -- they are ~5s and a broken
# comparator would make the land gate's verdict meaningless too.
if [ "$HALO_BATCH_COMMIT" = "1" ]; then
    CUR_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
    if [ "$CUR_BRANCH" = "main" ]; then
        echo "regression-test: HALO_BATCH_COMMIT ignored on main (no land gate follows)"
    else
        echo "regression-test: deferred to the land gate (HALO_BATCH_COMMIT=1, branch ${CUR_BRANCH:-detached})"
        echo "  auto_reintegrate.py Gate 5 runs the full suite over ${CUR_BRANCH:-HEAD} before main advances."
        exit 0
    fi
fi

# Leave a couple of cores for the rest of the commit; clamp to [1, 8] so a
# small box does not oversubscribe and a large one does not spawn 60 emulators.
JOBS=$(nproc 2>/dev/null || echo 1)
JOBS=$((JOBS - 2))
[ "$JOBS" -lt 1 ] && JOBS=1
[ "$JOBS" -gt 8 ] && JOBS=8

echo "Running Unicorn regression tests (quick, -j$JOBS, staged snapshot)..."
"$PY" "$SCRIPT" --quick -j "$JOBS" --staged
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "Unicorn regression test FAILED. Fix the divergence before committing."
    echo "Run: python3 tools/equivalence/regression_test.py --staged --no-cache  (full details, uncached)"
fi
exit $RC
