#!/usr/bin/env bash
# Claude Code WorktreeCreate hook: derive the new worktree path from stdin and
# copy the immutable raw-XBE oracle from the primary checkout.
set -euo pipefail

payload="$(cat)"
worktree="$(printf '%s' "$payload" | jq -er '.worktree_path // .worktreePath')" || {
    echo 'worktree assets: WorktreeCreate payload lacks worktree path' >&2
    exit 1
}
source_root="$(git worktree list --porcelain | awk 'NR == 1 { root = $2 } END { print root }')"
if [ -z "$source_root" ]; then
    echo 'worktree assets: could not find primary worktree' >&2
    exit 1
fi
"$source_root/tools/hooks/stage-worktree-assets.sh" "$source_root" "$worktree"
