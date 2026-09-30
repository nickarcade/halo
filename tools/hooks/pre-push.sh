#!/usr/bin/env bash
# Pre-push: run the same VC71 score-floor gate PR CI runs
# (.github/workflows/vc71-regression.yml) before anything reaches main.
#
# - Acts only on pushes to refs/heads/main; every other ref is skipped.
# - Scope is the whole pushed range (merge-base(remote, local)..local), not just
#   the last commit, and mirrors CI: changed src/*.c files, plus the source files
#   owning changed kb.json addresses, or every floor when shared code, headers,
#   CMake, or the reference/scoring tooling changed.
# - Runs `vc71_regression.py check --strict` (zero tolerated drop, fail closed on
#   missing evidence). Read-only: never calls `update`, never stages anything.
# - The check measures the WORKING TREE, so the pushed sha must be HEAD and the
#   files that feed the score must be clean; otherwise the result would describe
#   something other than what is being pushed.
# - Lowered/removed floors in vc71_scores.json are reported as a WARNING only.
#   CI rejects them, but the pre-commit raw-byte waiver lowers floors on purpose.
# - Also runs the full UNCACHED Unicorn sweep over the pushed tree (see below).
# - Emergency bypass: git push --no-verify  (CI remains the backstop).
set -uo pipefail

ZERO=0000000000000000000000000000000000000000
ROOT="$(git rev-parse --show-toplevel)"
REGR="$ROOT/tools/verify/vc71_regression.py"
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

    mapfile -t SOURCES < <(printf '%s\n' "${CHANGED[@]}" | grep -E '^src/.*\.c$' || true)
    FULL=0; KB=0
    for path in "${CHANGED[@]}"; do
        case "$path" in
            src/*.c) ;;
            kb.json) KB=1 ;;
            src/*|CMakeLists.txt|toolchains/*|tools/analysis/knowledge.py|tools/verify/compare_obj.py|tools/verify/vc71_cache.py|tools/verify/function_bounds.json|tools/verify/vc71_regression.py|tools/verify/vc71_verify.py|tools/verify/xbe_reference.py|tools/verify/vc71_scores.json)
                FULL=1 ;;
        esac
    done

    if [ "$FULL" -ne 1 ] && [ "$KB" -eq 1 ]; then
        mapfile -t KBSRC < <("$PY" "$ROOT/tools/audit/kb_diff_sources.py" --base "$BASE" --head "$local_sha")
        SOURCES+=("${KBSRC[@]}")
    fi
    if [ ${#SOURCES[@]} -gt 0 ]; then
        mapfile -t SOURCES < <(printf '%s\n' "${SOURCES[@]}" | grep -v '^$' | sort -u)
    fi
    if [ "$FULL" -ne 1 ] && [ ${#SOURCES[@]} -eq 0 ]; then
        continue   # nothing that can move a score
    fi

    # The check reads the working tree: it must be the pushed commit, unmodified.
    [ "$(git rev-parse HEAD)" = "$local_sha" ] \
        || fail "pushed sha is not HEAD; check out $local_sha to gate it (bypass: --no-verify)"
    if ! git diff --quiet HEAD -- src kb.json CMakeLists.txt toolchains tools/verify tools/analysis/knowledge.py; then
        fail "uncommitted changes in score inputs (src/, kb.json, tools/verify/...); commit or stash them first"
    fi
    [ -f "$ROOT/halo-patched/cachebeta.xbe" ] \
        || fail "halo-patched/cachebeta.xbe missing: the gate would measure nothing (bypass: --no-verify)"

    # Lowered/removed floors vs the remote tip: warn only (see header).
    if git cat-file -e "$BASE:tools/verify/vc71_scores.json" 2>/dev/null; then
        BASE_SCORES=$(mktemp)
        git show "$BASE:tools/verify/vc71_scores.json" > "$BASE_SCORES"
        "$PY" - "$BASE_SCORES" "$ROOT/tools/verify/vc71_scores.json" <<'PYEOF' >&2
import json, sys
def by_addr(p):
    out = {}
    for name, e in json.load(open(p, encoding="utf-8")).get("scores", {}).items():
        out.setdefault(e.get("addr") or "name:" + name, (name, e))
    return out
base, cand = by_addr(sys.argv[1]), by_addr(sys.argv[2])
msgs = []
for k, (n, b) in base.items():
    c = cand.get(k)
    if c is None:
        msgs.append("%s: removed (was %s)" % (n, b.get("score")))
    else:
        try:
            if float(c[1]["score"]) < float(b["score"]):
                msgs.append("%s: %s -> %s" % (c[0], b["score"], c[1]["score"]))
        except (KeyError, TypeError, ValueError):
            msgs.append("%s: invalid score entry" % c[0])
if msgs:
    print("pre-push WARNING: VC71 floors lowered/removed vs remote (CI rejects these):")
    for m in msgs[:50]:
        print("  - " + m)
PYEOF
        rm -f "$BASE_SCORES"
    fi

    if [ "$FULL" -eq 1 ]; then
        ARGS=(check --strict)
        echo "pre-push: VC71 strict check, all baselined TUs (shared code/tooling changed)..."
    else
        ARGS=(check --strict --source "${SOURCES[@]}")
        echo "pre-push: VC71 strict check, ${#SOURCES[@]} source file(s)..."
    fi
    if ! timeout 1800 "$PY" "$REGR" "${ARGS[@]}"; then
        echo "" >&2
        echo "pre-push: VC71 regression gate FAILED; push to main blocked." >&2
        echo "  Fix, or if the metric moved for a legitimate reason see the hints from" >&2
        echo "  pre-commit-vc71-regression.sh (update / rebaseline). Bypass: git push --no-verify" >&2
        exit 1
    fi
done
exit 0
