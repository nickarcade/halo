#!/usr/bin/env bash
# Claude Code WorktreeCreate hook: derive the new worktree path from stdin and
# copy the immutable raw-XBE oracle from the primary checkout.
set -euo pipefail

payload="$(cat)"
worktree="$(printf '%s' "$payload" | jq -er '.worktree_path // .worktreePath' 2>/dev/null || true)"
if [ -z "$worktree" ] && [ "$#" -ge 1 ]; then
    worktree="$1"
fi
source_root="$(git worktree list --porcelain | awk 'NR == 1 { root = $2 } END { print root }')"
if [ -z "$source_root" ]; then
    echo 'worktree assets: could not find primary worktree' >&2
    exit 1
fi

case "$worktree" in
    ""|null|None)
        # Command WorktreeCreate hooks own worktree creation.  Some Claude
        # Code versions provide no path in this event, so create one and
        # return it on stdout for the harness to use.
        worktree_root="$source_root/.claude/worktrees"
        mkdir -p "$worktree_root"
        worktree="$(mktemp -d "$worktree_root/agent-XXXXXXXX")"
        branch="worktree-$(basename "$worktree")"
        rmdir "$worktree"
        if ! git -C "$source_root" worktree add -b "$branch" "$worktree" HEAD >&2; then
            echo 'worktree assets: could not create worktree' >&2
            exit 1
        fi
        ;;
esac
"$source_root/tools/hooks/stage-worktree-assets.sh" "$source_root" "$worktree" >&2
printf '%s\n' "$worktree"
