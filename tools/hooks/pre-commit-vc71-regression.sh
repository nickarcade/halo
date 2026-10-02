#!/usr/bin/env bash
# HALO-HOOK-TRIGGER: ^(src/|kb\.json$|CMakeLists\.txt$|toolchains/|third_party/xbox/|tools/analysis/|tools/verify/|tools/audit/check_delinked_bounds\.py$|tools/equivalence/xbe_image\.py$)
# Compatibility filename: the gate now checks fresh raw-XBE aligned bytes.
# No mnemonic floors are updated or auto-staged. The dispatcher retains its
# legacy phase variable; its update phase has no work after the byte gate.
set -euo pipefail
[ "${HALO_VC71_PHASE:-check}" = update ] && exit 0
ROOT="$(git rev-parse --show-toplevel)"
PY="$ROOT/.venv/bin/python3"; [ -x "$PY" ] || PY=python3
exec "$PY" "$ROOT/tools/verify/byte_regression.py" local
