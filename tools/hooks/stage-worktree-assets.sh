#!/usr/bin/env bash
# Copy read-only oracle assets into a new Git worktree.
#
# Worktrees must never symlink halo-patched/ or delinked/ from the primary
# checkout: build output is writable, and a checkout clean can follow a link
# and delete host-only assets. The pristine cachebeta.xbe is the only required
# shared input for the VC71 and raw-XBE verification lanes, so copy it.
set -euo pipefail

readonly PRISTINE_XBE_MD5='c7869590a1c64ad034e49a5ee0c02465'

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <source-repository> <worktree>" >&2
    exit 2
fi

source_root="$(cd "$1" && pwd -P)"
worktree="$(cd "$2" && pwd -P)"
source_xbe="$source_root/halo-patched/cachebeta.xbe"
target_dir="$worktree/halo-patched"
target_xbe="$target_dir/cachebeta.xbe"

if [ "$source_root" = "$worktree" ]; then
    exit 0
fi
if [ ! -f "$source_xbe" ]; then
    echo "worktree assets: source XBE missing: $source_xbe" >&2
    exit 1
fi
if [ "$(md5sum "$source_xbe" | awk '{print $1}')" != "$PRISTINE_XBE_MD5" ]; then
    echo "worktree assets: source XBE has unexpected MD5: $source_xbe" >&2
    exit 1
fi
if [ -L "$target_dir" ]; then
    echo "worktree assets: refusing unsafe halo-patched symlink: $target_dir" >&2
    exit 1
fi
mkdir -p "$target_dir"
if [ -e "$target_xbe" ] && [ ! -f "$target_xbe" ]; then
    echo "worktree assets: target is not a regular file: $target_xbe" >&2
    exit 1
fi
if [ ! -f "$target_xbe" ] || [ "$(md5sum "$target_xbe" | awk '{print $1}')" != "$PRISTINE_XBE_MD5" ]; then
    cp --reflink=auto --preserve=mode,timestamps "$source_xbe" "$target_xbe"
fi
if [ "$(md5sum "$target_xbe" | awk '{print $1}')" != "$PRISTINE_XBE_MD5" ]; then
    echo "worktree assets: copied XBE failed MD5 verification: $target_xbe" >&2
    exit 1
fi
printf 'worktree assets: staged pristine XBE -> %s\n' "$target_xbe"
