#!/usr/bin/env bash
# Idempotent git-hook installer for this repo.
#
# Git hooks live in tools/hooks/ and are activated with
#   git config core.hooksPath tools/hooks
# Git runs hooks only by exact name (pre-commit, commit-msg, post-checkout, ...),
# so each hook has a thin entry point of that name next to its implementation
# script (<name>.sh). Nothing is written to .git/hooks.
#
# Besides setting core.hooksPath this runs post-checkout once, which trusts the
# RTK project filters (`rtk trust`) and creates the CLAUDE.md -> AGENTS.md
# symlink if it is missing.
#
# The post-commit and pre-push hooks are opt-in (maintainer-local, heavy work):
#   export HALO_HEAVY_HOOKS=1
#
# Usage:
#   tools/hooks/install.sh          install/repair (idempotent)
#   tools/hooks/install.sh --check  report drift only; exit 1 on drift; no mutation
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

HOOK_NAMES="pre-commit commit-msg prepare-commit-msg post-checkout post-commit pre-push"

report() { echo "  [hooks] $*"; }

# ------------------------------------------------------------------ drift scan
drift=0
current="$(git config --get core.hooksPath || true)"
if [ "$current" != "tools/hooks" ]; then
    drift=1
    [ "$CHECK_ONLY" = 1 ] && report "DRIFT: core.hooksPath is '${current:-<unset>}', want tools/hooks"
fi
for name in $HOOK_NAMES; do
    if [ ! -f "$REPO_ROOT/tools/hooks/$name" ]; then
        drift=1
        [ "$CHECK_ONLY" = 1 ] && report "DRIFT: tools/hooks/$name is missing"
    elif [ ! -x "$REPO_ROOT/tools/hooks/$name" ]; then
        drift=1
        [ "$CHECK_ONLY" = 1 ] && report "DRIFT: tools/hooks/$name is not executable"
    fi
done

if [ "$CHECK_ONLY" = 1 ]; then
    if [ "$drift" = 1 ]; then
        report "hook drift detected — run: tools/hooks/install.sh"
        exit 1
    fi
    exit 0    # silent on success (keeps SessionStart quiet)
fi

# ------------------------------------------------------------------ install
if [ "$current" != "tools/hooks" ]; then
    git config core.hooksPath tools/hooks
    report "set core.hooksPath = tools/hooks"
fi
for name in $HOOK_NAMES; do
    [ -f "$REPO_ROOT/tools/hooks/$name" ] && chmod +x "$REPO_ROOT/tools/hooks/$name"
done

# Run post-checkout once: rtk trust + CLAUDE.md symlink.
bash "$REPO_ROOT/tools/hooks/post-checkout.sh" || true

report "install complete"
exit 0
