#!/usr/bin/env bash
# Pre-push: fresh base-versus-pushed-commit raw-XBE aligned byte regression gate.
# Isolated Git snapshots preserve unrelated working-tree and index changes.
# Also retains the independent uncached Unicorn behavioral sweep.
set -uo pipefail

ZERO=0000000000000000000000000000000000000000
ROOT="$(git rev-parse --show-toplevel)"
REGR="$ROOT/tools/verify/byte_regression.py"
PY="$ROOT/.venv/bin/python3"; [ -x "$PY" ] || PY=python3

[ -f "$REGR" ] || exit 0

fail() { echo "pre-push: $*" >&2; exit 1; }

while read -r local_ref local_sha remote_ref remote_sha; do
    [ "$remote_ref" = "refs/heads/main" ] || continue
    [ "$local_sha" = "$ZERO" ] && continue   # deletion

    if [ "$remote_sha" = "$ZERO" ] || ! git cat-file -e "$remote_sha^{commit}" 2>/dev/null; then
        BASE=$(git merge-base "$local_sha" "$(git rev-parse -q --verify refs/remotes/my/main || echo "$local_sha")" 2>/dev/null) || BASE=""
    else
        BASE=$(git merge-base "$remote_sha" "$local_sha" 2>/dev/null) || BASE=""
    fi
    [ -n "$BASE" ] || fail "cannot determine a base for $remote_ref; refusing (bypass: --no-verify)"

    mapfile -t CHANGED < <(git diff --name-only "$BASE" "$local_sha")
    [ ${#CHANGED[@]} -gt 0 ] || continue

    # Full Unicorn differential sweep, UNCACHED, over the pushed commit's tree.
    # The pre-commit gate reuses verdicts by content key; this is the independent
    # from-scratch backstop for anything that reaches main (same targets, seeds and
    # verdict rules, compiled from the pushed tree with the CMake Release flags).
    # Skipped when nothing the harness reads changed, or unicorn is not installed.
    # CI (equivalence.yml) and any nightly job should run the same command:
    #   tools/equivalence/regression_test.py --quick -j8 --snapshot <sha> --no-cache
    if printf '%s\n' "${CHANGED[@]}" | grep -qE '^(src/|kb\.json$|tools/equivalence/|tools/verify/function_bounds\.json$)' \
        && [ -f "$ROOT/tools/equivalence/regression_test.py" ] \
        && "$PY" -c 'import unicorn' 2>/dev/null; then
        UJOBS=$(( $(nproc 2>/dev/null || echo 1) - 2 )); [ "$UJOBS" -lt 1 ] && UJOBS=1; [ "$UJOBS" -gt 8 ] && UJOBS=8
        echo "pre-push: full Unicorn sweep (uncached, -j$UJOBS) over $local_sha..."
        if ! timeout 1800 "$PY" "$ROOT/tools/equivalence/regression_test.py" --quick -j "$UJOBS" \
                --snapshot "$local_sha" --no-cache; then
            echo "" >&2
            echo "pre-push: Unicorn regression sweep FAILED; push to main blocked." >&2
            echo "  Details: python3 tools/equivalence/regression_test.py --snapshot $local_sha --no-cache" >&2
            echo "  Emergency bypass: git push --no-verify (CI remains the backstop)" >&2
            exit 1
        fi
    fi

    echo "pre-push: fresh raw-XBE byte regression check over $BASE..$local_sha..."
    if ! timeout 7200 "$PY" "$REGR" local --base-ref "$BASE" --head-ref "$local_sha"; then
        fail "raw-XBE byte regression gate failed; inspect its measurement artifacts"
    fi
done
exit 0
