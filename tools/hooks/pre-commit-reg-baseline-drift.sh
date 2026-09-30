#!/usr/bin/env bash
# HALO-HOOK-TRIGGER: kb\.json|kb_reg_baseline\.json
# Pre-commit hook: detect drift between kb.json @<reg> annotations and
# tools/kb_reg_baseline.json. Runs only when the staged diff touches kb.json
# or the baseline itself, so unrelated commits never get blocked by upstream
# drift introduced elsewhere.

. "$(dirname "${BASH_SOURCE[0]}")/lib-staged.sh"
STAGED=$(staged_list all)
case "$STAGED" in
    *kb.json*|*kb_reg_baseline.json*) ;;
    *) exit 0 ;;
esac

python3 tools/audit/extract_reg_args.py --check >/tmp/reg-drift-stdout.txt 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "ERROR: kb_reg_baseline drift detected."
    cat /tmp/reg-drift-stdout.txt
    echo ""
    echo "Run: tools/audit/extract_reg_args.py --apply  (then stage kb_reg_baseline.json)"
    echo "Or:  git commit --no-verify  (emergency bypass)"
    exit 1
fi

exit 0
