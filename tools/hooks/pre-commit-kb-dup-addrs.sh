#!/usr/bin/env bash
# Pre-commit hook: every function addr and every data addr in kb.json must
# appear exactly once.
#
# A duplicate addr is silent: the build still links (identical decls are
# legal C) and tools that key {addr: fn} just keep the last copy. It reaches
# kb.json through name-keyed inserts into one of the objects whose names are
# allowlisted as duplicates (<xdk_stubs>, scenario.obj): 070ecec43 added 11
# Bink callees to BOTH <xdk_stubs> objects and it went unnoticed onto main.
# The remedy is to keep the entry in the object that owns its neighbours and
# drop the other copy.
#
# Bypass: git commit --no-verify

if ! git diff --cached --name-only | grep -qx 'kb.json'; then
    exit 0
fi

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
if ! git show ":kb.json" > "$TMP" 2>/dev/null; then
    echo "kb-dup-addrs: cannot read staged kb.json; skipping." 1>&2
    exit 0
fi

python3 - "$TMP" <<'EOF' 1>&2
import json, sys
from collections import defaultdict

kb = json.load(open(sys.argv[1]))
bad = 0
for kind in ("functions", "data"):
    where = defaultdict(list)
    for i, obj in enumerate(kb["objects"]):
        for entry in obj.get(kind) or []:
            where[entry["addr"]].append("objects[%d] %s" % (i, obj.get("name")))
    for addr, locs in sorted(where.items(), key=lambda kv: int(kv[0], 16)):
        if len(locs) > 1:
            bad += 1
            print("kb-dup-addrs: %s %s appears %d times: %s"
                  % (kind[:-1] if kind == "functions" else kind, addr,
                     len(locs), "; ".join(locs)))
sys.exit(1 if bad else 0)
EOF
RC=$?
if [ $RC -ne 0 ]; then
    echo "" 1>&2
    echo "Commit blocked: kb.json has duplicate addrs (see above)." 1>&2
    echo "Bypass with: git commit --no-verify" 1>&2
fi
exit $RC
