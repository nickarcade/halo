#!/usr/bin/env bash
# Pre-commit: run vc71_regression.py check on the staged work, then refresh the
# floor with any improvements or new ports and re-stage scores.
#
# - Fires when src/**/*.c files are staged, OR when the reference table itself
#   (tools/verify/function_bounds.json) is staged. The second trigger exists
#   because editing that table IS editing every reference cut from it: scores
#   move with no .c file touched, and a src-only trigger would never run.
# - Files not yet in the baseline are silently skipped by `check` (no false
#   positives on new ports).
# - `check` fails on three things: a score drop past the threshold, a reference
#   change (measured ref_sha != the floor's ref_sha -- the numbers describe
#   different bytes), and silence (a baselined function this run did not measure
#   at all). A whole-TU compile failure is reported but not fatal: a checkout
#   without the RXDK/wine VC71 toolchain fails every compile, and a gate that
#   blocks every commit there is a gate people turn off.
# - After `check` passes, `update` records new ports and raises any improved
#   floors. If vc71_scores.json changed, it's auto-staged into the commit so
#   the progress dashboard stays in sync without a separate step.
# - To bypass in an emergency: git commit --no-verify
#
# COST. Both passes below are memoized: run_vc71_verify caches each TU's
# measurement under (tool epoch + source sha + the TU's decl.h slice), so
# `update` reuses what `check` just measured instead of recompiling everything a
# second time, and a hook killed by a timeout does not start over. The key covers
# every input that can move a score EXCEPT the VC71 toolchain environment itself
# (wine/CL), which is why compile failures are never cached. If you have just
# repaired that environment and want to force a clean re-measure:
#   VC71_NO_MEASURE_MEMO=1 git commit ...
# or delete artifacts/audit/vc71_measure_cache.json.

STAGED=$(git diff --cached --name-only --diff-filter=ACMR)
STAGED_SRC=$(echo "$STAGED" | grep -E '^src/.*\.c$')
STAGED_BOUNDS=$(echo "$STAGED" | grep -Fx 'tools/verify/function_bounds.json')

if [ -z "$STAGED_SRC" ] && [ -z "$STAGED_BOUNDS" ]; then
    exit 0
fi

ROOT="$(git rev-parse --show-toplevel)"
REGR="$ROOT/tools/verify/vc71_regression.py"
SCORES="$ROOT/tools/verify/vc71_scores.json"

if [ ! -f "$REGR" ]; then
    exit 0
fi

SRC_ARGS=()
while IFS= read -r f; do
    [ -n "$f" ] && SRC_ARGS+=("$f")
done <<< "$STAGED_SRC"

# A staged bounds edit moves the reference for every baselined function on a
# changed entry. bounds-gate maps those entries back to their source files and
# exits non-zero when the commit does not also carry a re-baselined floor -- the
# per-function ref_sha pin in `check` then verifies the rebaseline is real.
if [ -n "$STAGED_BOUNDS" ]; then
    BOUNDS_SRC=$(python3 "$REGR" bounds-gate)
    RC=$?
    if [ $RC -ne 0 ]; then
        echo ""
        echo "Emergency bypass (records no floor update): git commit --no-verify"
        exit $RC
    fi
    while IFS= read -r f; do
        [ -n "$f" ] && SRC_ARGS+=("$f")
    done <<< "$BOUNDS_SRC"
fi

# De-duplicate: a file can be both staged and bounds-affected.
if [ ${#SRC_ARGS[@]} -gt 0 ]; then
    mapfile -t SRC_ARGS < <(printf '%s\n' "${SRC_ARGS[@]}" | sort -u)
fi
if [ ${#SRC_ARGS[@]} -eq 0 ]; then
    exit 0
fi

echo "vc71-regression: checking ${#SRC_ARGS[@]} source file(s)..."
python3 "$REGR" check --source "${SRC_ARGS[@]}" --quiet
RC=$?

# Raw-byte waiver. VC71 mnemonic % is the coarser metric. A function whose raw
# aligned bytes against the XBE went UP, in a TU where no function lost any, may
# lower its own mnemonic floor. raw_waiver.py measures HEAD and the index
# itself; nothing is taken on trust. Only those functions are waived and lowered.
LOWER=()
if [ $RC -ne 0 ]; then
    PY="$ROOT/.venv/bin/python"; [ -x "$PY" ] || PY=python3
    WAIVER_JSON=$(mktemp)
    if "$PY" "$ROOT/tools/verify/raw_waiver.py" --source "${SRC_ARGS[@]}" --json "$WAIVER_JSON"; then
        mapfile -t LOWER < <("$PY" -c 'import json,sys
for row in json.load(open(sys.argv[1])).values():
    for fn in row.get("waivable", []): print(fn)' "$WAIVER_JSON")
    fi
    rm -f "$WAIVER_JSON"
    if [ ${#LOWER[@]} -gt 0 ]; then
        echo "vc71-regression: re-checking with raw-byte waiver for: ${LOWER[*]}"
        python3 "$REGR" check --source "${SRC_ARGS[@]}" --quiet --waive "${LOWER[@]}"
        RC=$?
    fi
fi
if [ $RC -ne 0 ]; then
    echo ""
    echo "A score drop: investigate, fix, then update the floor:"
    echo "  python3 tools/verify/vc71_regression.py update --source <file>"
    echo ""
    echo "MEASUREMENT CHANGED (the reference moved, not the lift): do NOT lower"
    echo "the floor. Review, then re-baseline the affected TUs:"
    echo "  python3 tools/verify/vc71_regression.py populate --rebaseline --source <file>"
    echo "  python3 tools/verify/vc71_regression.py rebaseline-report"
    echo ""
    echo "UNMEASURED (a baselined function produced no score): it was deleted,"
    echo "renamed, or is no longer compiled into that TU. Re-baseline the TU if"
    echo "that was intended."
    echo ""
    echo "Emergency bypass (records no floor update): git commit --no-verify"
    exit $RC
fi

# Check passed: promote any new/improved scores into the committed JSON so
# the progress dashboard reflects the lift on the same commit. Update failure
# here is non-blocking — check already validated the floors, this is just a
# best-effort sync of the dashboard data.
SCORES_BEFORE=""
if [ -f "$SCORES" ]; then
    SCORES_BEFORE=$(git hash-object "$SCORES")
fi
LOWER_ARGS=()
[ ${#LOWER[@]} -gt 0 ] && LOWER_ARGS=(--lower "${LOWER[@]}")
python3 "$REGR" update --source "${SRC_ARGS[@]}" "${LOWER_ARGS[@]}" >/dev/null 2>&1 || {
    echo "vc71-regression: update failed (non-blocking); scores not refreshed"
    exit 0
}
SCORES_AFTER=""
if [ -f "$SCORES" ]; then
    SCORES_AFTER=$(git hash-object "$SCORES")
fi
if [ "$SCORES_BEFORE" != "$SCORES_AFTER" ]; then
    git add "$SCORES"
    echo "vc71-regression: auto-staged refreshed vc71_scores.json"
fi
exit 0
