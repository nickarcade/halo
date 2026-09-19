#!/bin/sh
# .git/hooks/prepare-commit-msg — auto-generate lift commit messages
# Install: cp tools/prepare-commit-msg-hook.sh .git/hooks/prepare-commit-msg && chmod +x .git/hooks/prepare-commit-msg
#
# When kb.json, kb_meta.json, or src/**/*.c files are staged, this hook
# prepopulates the commit message with the output of generate_lift_commit.py.
# The user (or agent) can then review and confirm.

COMMIT_MSG_FILE=$1
COMMIT_SOURCE=$2

# When squashing, auto-consolidate lift commits into the repo standard format
if [ "$COMMIT_SOURCE" = "squash" ]; then
    if [ -x "$(command -v python3)" ] && [ -f "tools/audit/consolidate_squash_msg.py" ]; then
        if python3 tools/audit/consolidate_squash_msg.py --check "$COMMIT_MSG_FILE" 2>/dev/null; then
            python3 tools/audit/consolidate_squash_msg.py --in-place "$COMMIT_MSG_FILE" 2>/dev/null
        fi
    fi
    exit 0
fi

# Only prepopulate when the user is about to get an EDITOR with no message:
# COMMIT_SOURCE is empty for plain `git commit`, and set for every explicit
# source — "message" (-m/-F), "commit" (--amend/-c/-C), "template" (-t),
# "merge". Overwriting an explicit -m/-F message silently replaced
# real cleanup/maintenance messages with generated lift messages (2026-07-08).
if [ -n "$COMMIT_SOURCE" ]; then
    exit 0
fi

# Check if lift-related files are staged
if git diff --cached --name-only | grep -qE '^(kb\.json|kb_meta\.json|src/.*\.c)$'; then
    # Git runs pre-commit BEFORE prepare-commit-msg, so when kb.json is staged
    # tools/hooks/pre-commit-lift-audit.sh has already run the ABI audit and the
    # kb_reg_baseline drift gate against this exact index and aborted the commit
    # if either failed. Re-running them here can only reach the same verdict,
    # and this hook discards stderr and ignores the exit code anyway, so their
    # cost buys nothing. When kb.json is NOT staged that hook exits early, so
    # the gates must still run here.
    SKIP_GATES=""
    if git diff --cached --name-only | grep -qx 'kb.json'; then
        SKIP_GATES="--skip-abi-audit"
    fi

    # Generate the message and prepend it
    if [ -x "$(command -v python3)" ] && [ -f "tools/audit/generate_lift_commit.py" ]; then
        GENERATED=$(python3 tools/audit/generate_lift_commit.py $SKIP_GATES 2>/dev/null)
        if [ -n "$GENERATED" ]; then
            echo "$GENERATED" > "$COMMIT_MSG_FILE"
            echo "" >> "$COMMIT_MSG_FILE"
            echo "# Please review the generated message above." >> "$COMMIT_MSG_FILE"
            echo "# If it looks correct, save and quit." >> "$COMMIT_MSG_FILE"
            echo "# If it is empty or wrong, check that all changes are staged." >> "$COMMIT_MSG_FILE"
        fi
    fi
fi

exit 0
