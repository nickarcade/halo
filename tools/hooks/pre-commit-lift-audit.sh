#!/usr/bin/env bash
# Pre-commit hook: if staged changes touch lift-sensitive files (src/**/*.c + kb.json),
# run generate_lift_commit.py in dry-run mode to gate on ABI audit.
# Skipped for non-lift commits (docs, tools-only, etc).

STAGED=$(git diff --cached --name-only)

HAS_C=$(echo "$STAGED" | grep -E '^src/.*\.c$')
HAS_KB=$(echo "$STAGED" | grep -qx 'kb.json' && echo "yes")

if [ -z "$HAS_C" ] && [ -z "$HAS_KB" ]; then
    exit 0
fi

if [ -z "$HAS_KB" ]; then
    exit 0
fi

echo "lift-audit: checking staged kb.json changes..."
# --gate-only: this hook consumes the exit code and the stderr below and sends
# stdout to /dev/null, so the commit message was built and thrown away on every
# commit. Generating it re-runs vc71_verify once per staged port (measured
# 9.6-12.9s for a five-function port) plus two artifacts/ directory scans. The
# gates themselves (cross-TU staging, ABI audit, kb_reg_baseline drift,
# sync-ported warning) all run before that point and are unaffected.
python3 tools/audit/generate_lift_commit.py --gate-only --batch-name "_preflight" > /dev/null 2>/tmp/lift-audit-stderr.txt
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "ERROR: ABI audit failed for staged lift changes."
    cat /tmp/lift-audit-stderr.txt
    echo ""
    echo "Fix the issue, or use: git commit --no-verify"
    exit 1
fi

WARNINGS=$(grep -i "warn" /tmp/lift-audit-stderr.txt 2>/dev/null)
if [ -n "$WARNINGS" ]; then
    echo "$WARNINGS"
fi

exit 0
