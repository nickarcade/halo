#!/usr/bin/env bash
# Assert the one rule that keeps a self-hosted job from eating the dev tree.
#
# RULE: a runner-workspace path may be a symlink into /mnt/g/dev/halo only if
#       .gitignore ignores that path AS A WHOLE DIRECTORY.
#
# `actions/checkout` cleans the workspace on every run (`git clean -ffdx`).
# For a link git considers ignored, it removes the link and stops.  For a link
# git does NOT consider ignored it must look inside to decide what to clean, so
# it follows the link into the live tree and deletes the untracked files it
# finds there.
#
# That is not hypothetical.  On 2026-09-16 delinked/ held 8 tracked .obj files,
# which made `/delinked/` unusable as an ignore rule for the directory itself;
# the clean followed the link and deleted every delinked object on the host
# except those 8.  halo-patched/ came through intact because `/halo-patched`
# ignores the whole path.
#
# The three assets that were damaged are now COPIED rather than linked (see
# .github/actions/stage-local-assets).  Links remain only for build/ (44M) and
# artifacts/<dir> (215M), where copying per run is not worth it -- this check
# is what keeps those two safe, and catches any new link added later.
#
# Usage: tools/ci/check_workspace_links.sh [dir]   (default: .)

set -uo pipefail

root="${1:-.}"
fail=0

while IFS= read -r link; do
    target="$(readlink -f -- "$link" 2>/dev/null || true)"
    rel="${link#./}"

    # Only links escaping the workspace can damage anything outside it.
    case "$target" in
        "$(readlink -f -- "$root")"/*) continue ;;
        "") continue ;;
    esac

    if git check-ignore -q -- "$rel"; then
        echo "ok:   $rel -> $target (ignored as a whole path)"
    else
        echo "::error::$rel points outside the workspace at $target but is NOT"
        echo "::error::ignored as a whole path, so the checkout clean will follow"
        echo "::error::it and delete untracked files in the live tree."
        echo "::error::Fix: copy it in (.github/actions/stage-local-assets), or"
        echo "::error::add an ignore rule that covers '$rel' itself."
        fail=1
    fi
done < <(find "$root" -path "$root/.git" -prune -o -type l -print 2>/dev/null)

if [ "$fail" -eq 0 ]; then
    echo "workspace link check: no escaping link is unignored"
fi
exit "$fail"
