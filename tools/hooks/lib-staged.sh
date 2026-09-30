#!/usr/bin/env bash
# Shared helper for the pre-commit gates (sourced, not executed).
#
# The dispatcher (pre-commit.sh) asks git for the staged paths ONCE, writes the
# lists under $HALO_STAGED_DIR and exports that variable. Each gate then reads the
# list instead of forking its own `git diff --cached` (each ~0.3-0.5s on the 9p
# checkout, 14+ per commit). Standalone runs (no dispatcher, or the directory
# gone) fall back to the original git invocation, so a gate behaves identically
# either way.
#
#   staged_list all    git diff --cached --name-only
#   staged_list acmr   git diff --cached --name-only --diff-filter=ACMR
#   staged_list acm    git diff --cached --name-only --diff-filter=ACM
#   staged_list notd   git diff --cached --name-only --diff-filter=d   (not deleted)
staged_list() {
    local kind="${1:-all}"
    if [ -n "${HALO_STAGED_DIR:-}" ] && [ -f "$HALO_STAGED_DIR/$kind" ]; then
        cat "$HALO_STAGED_DIR/$kind"
        return 0
    fi
    case "$kind" in
        all)  git diff --cached --name-only ;;
        acmr) git diff --cached --name-only --diff-filter=ACMR ;;
        acm)  git diff --cached --name-only --diff-filter=ACM ;;
        notd) git diff --cached --name-only --diff-filter=d ;;
    esac
}
