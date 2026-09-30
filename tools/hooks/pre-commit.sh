#!/usr/bin/env bash
# Composite pre-commit hook — runs all checks in tools/hooks/pre-commit-*.sh
#
# This script is invoked via the symlink .git/hooks/pre-commit, so $0 (and
# BASH_SOURCE) point at .git/hooks — NOT at this file's real location. Deriving
# DIR from $0 therefore globbed .git/hooks/pre-commit-*.sh (zero matches) and
# silently ran NONE of the gates. Resolve tools/hooks from the repo root instead.
#
# STAGED LIST, ONCE. `git diff --cached --name-status` is asked a single time and
# fanned out as four lists under $HALO_STAGED_DIR (all / acmr / acm / notd, the
# filters the gates used to request one by one); hooks read them through
# lib-staged.sh and fall back to their own git call when run standalone.
#
# TRIGGERS. A hook may declare `# HALO-HOOK-TRIGGER: <ERE>` in its first lines.
# If no staged path (unfiltered list, a superset of every hook's own filter) matches
# it, the dispatcher does not start the hook at all. Hooks still re-check their own
# precise condition, so the header can only ever SKIP work a hook would itself have
# skipped. No header, or an unreadable staged list, means the hook runs.
#
# CONCURRENCY. Everything runs at the same time:
#   * the read-only gates, HALO_HOOK_JOBS at a time (default 6; 1 = serial);
#   * the two heavy gates (Unicorn regression-test, VC71 check) in their own
#     background slots from the very start, so they overlap the light gates too.
# Each hook's output is buffered to its own file and printed in a fixed order
# (glob order, then regression-test, then vc71-regression), so output never
# interleaves; every failure is reported, and the exit code is the first non-zero
# in that same order.
#
# vc71-regression is split (HALO_VC71_PHASE): its `check` runs concurrently, but the
# `update` (which rewrites and stages vc71_scores.json) only runs after every gate
# has passed -- exactly as when the tail was serial, a rejected commit leaves the
# index untouched.
#
# HALO_HOOK_TIMING=1 prints per-hook wall time to stderr.

DIR="$(git rev-parse --show-toplevel)/tools/hooks"
JOBS="${HALO_HOOK_JOBS:-6}"
HEAVY=" pre-commit-regression-test.sh pre-commit-vc71-regression.sh "

OUT="$(mktemp -d "${TMPDIR:-/tmp}/pre-commit.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT

# --- staged lists (one git call) -------------------------------------------
STAGED_DIR="$OUT/staged"
mkdir -p "$STAGED_DIR"
STAGED_KNOWN=0
if git diff --cached --name-status -z >"$OUT/name-status.z" 2>/dev/null; then
    : >"$STAGED_DIR/all"; : >"$STAGED_DIR/acmr"; : >"$STAGED_DIR/acm"; : >"$STAGED_DIR/notd"
    while IFS= read -r -d '' st; do
        case "$st" in
            R*|C*) IFS= read -r -d '' _old; IFS= read -r -d '' path ;;
            *)     IFS= read -r -d '' path ;;
        esac
        printf '%s\n' "$path" >>"$STAGED_DIR/all"
        case "${st:0:1}" in
            A|C|M|R) printf '%s\n' "$path" >>"$STAGED_DIR/acmr" ;;
        esac
        case "${st:0:1}" in
            A|C|M) printf '%s\n' "$path" >>"$STAGED_DIR/acm" ;;
        esac
        case "${st:0:1}" in
            D) ;;
            *) printf '%s\n' "$path" >>"$STAGED_DIR/notd" ;;
        esac
    done <"$OUT/name-status.z"
    STAGED_KNOWN=1
    export HALO_STAGED_DIR="$STAGED_DIR"
fi

# wants_hook <hook path>: 0 = run it, 1 = its trigger cannot match the staged set.
wants_hook() {
    [ "$STAGED_KNOWN" -eq 1 ] || return 0
    local trig
    trig=$(sed -n '2,6s/^# HALO-HOOK-TRIGGER: //p' "$1" | head -1)
    [ -n "$trig" ] || return 0
    grep -Eq -- "$trig" "$STAGED_DIR/all"
}

PARALLEL=()
HEAVY_HOOKS=()
for hook in "$DIR"/pre-commit-*.sh; do
    [ -x "$hook" ] || continue
    wants_hook "$hook" || continue
    case "$HEAVY" in
        *" $(basename "$hook") "*) HEAVY_HOOKS+=("$hook") ;;
        *) PARALLEL+=("$hook") ;;
    esac
done

now() { date +%s.%N; }

# run_hook <hook> <tag> [env assignments...]: buffered run -> $OUT/<tag>.{out,rc,t}
run_hook() {
    local hook="$1" tag="$2" t0 rc
    shift 2
    t0=$(now)
    env "$@" "$hook" </dev/null >"$OUT/$tag.out" 2>&1
    rc=$?
    echo "$rc" >"$OUT/$tag.rc"
    echo "$(now) $t0" >"$OUT/$tag.t"
}

# Heavy gates first: they are the critical path.
for i in "${!HEAVY_HOOKS[@]}"; do
    hook="${HEAVY_HOOKS[$i]}"
    if [ "$(basename "$hook")" = "pre-commit-vc71-regression.sh" ]; then
        run_hook "$hook" "heavy$i" HALO_VC71_PHASE=check HALO_VC71_STATE="$OUT/vc71.state" &
    else
        run_hook "$hook" "heavy$i" &
    fi
done

for i in "${!PARALLEL[@]}"; do
    # JOBS light gates at a time, on top of the heavy ones.
    while [ "$(jobs -rp | wc -l)" -ge "$(( JOBS + ${#HEAVY_HOOKS[@]} ))" ]; do
        wait -n
    done
    run_hook "${PARALLEL[$i]}" "p$i" &
done
wait

RC=0
report() { # report <tag> <hook>
    local tag="$1" hook="$2" rc t0 t1
    cat "$OUT/$tag.out"
    rc=$(cat "$OUT/$tag.rc" 2>/dev/null || echo 1)
    if [ -n "${HALO_HOOK_TIMING:-}" ] && [ -f "$OUT/$tag.t" ]; then
        read -r t1 t0 <"$OUT/$tag.t"
        printf 'hook-timing: %-40s %6.1fs rc=%s\n' "$(basename "$hook")" \
            "$(echo "$t1 - $t0" | bc)" "$rc" >&2
    fi
    if [ "$RC" -eq 0 ] && [ "$rc" -ne 0 ]; then
        RC=$rc
    fi
}

for i in "${!PARALLEL[@]}"; do
    report "p$i" "${PARALLEL[$i]}"
done
for i in "${!HEAVY_HOOKS[@]}"; do
    report "heavy$i" "${HEAVY_HOOKS[$i]}"
done
if [ $RC -ne 0 ]; then
    exit $RC
fi

# Everything passed: only now may vc71 rewrite and stage vc71_scores.json.
for hook in "${HEAVY_HOOKS[@]}"; do
    if [ "$(basename "$hook")" = "pre-commit-vc71-regression.sh" ]; then
        HALO_VC71_PHASE=update HALO_VC71_STATE="$OUT/vc71.state" "$hook"
        RC=$?
        [ $RC -ne 0 ] && exit $RC
    fi
done

exit 0
