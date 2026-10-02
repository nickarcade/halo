#!/usr/bin/env bash
set -euo pipefail
NAME="${0##*/}"
VENV="${VENV_PYTHON:-.venv/bin/python3}"

usage() {
    echo "Usage: $NAME [--batch] [--raw [--raw-workers N]] [--mnemonic|--full-mnemonic] [--run-id NAME]"
    echo ""
    echo "Refresh the local progress dashboard from the latest build."
    echo ""
    echo "By default this ONLY re-renders the dashboard from the existing"
    echo "scores — it does NOT run VC71 verification.  Verification is expensive"
    echo "and now happens at lift time (the lift pipeline runs an incremental"
    echo "analysis for the TU it just changed); byte regression hooks compile isolated snapshots, so a"
    echo "dashboard refresh no longer needs to re-verify anything.  Opt in with"
    echo "--raw to recompute byte accuracy; --mnemonic is optional similarity analysis."
    echo ""
    echo "  --batch       Run batch equivalence tests first (slow; skips existing results)."
    echo "  --mnemonic    Refresh mnemonic similarity (NOT raw byte accuracy) incrementally"
    echo "               (only TUs whose inputs changed are re-verified)."
    echo "  --full-mnemonic  Refresh mnemonic similarity for every TU (NOT raw byte accuracy)."
    echo "  --skip-vc71   Accepted for back-compat; now the default (no-op)."
    echo "  --vc71 / --full-vc71  Compatibility aliases for --raw (byte accuracy)."
    echo "  --raw         Refresh raw-XBE aligned byte accuracy for every ported"
    echo "               function before rendering (fresh byte_regression measurement;"
    echo "               slow: recompiles every TU)."
    echo "  --raw-workers N  Parallel workers for --raw (default 4)."
    echo "  --run-id NAME Tag the snapshot with a custom run label."
    echo "               Default: 'local-<timestamp>'"
    echo "  --help        This message. --batch combines with byte or mnemonic refresh."
    exit 0
}

RUN_ID="local-$(date +%Y%m%d-%H%M%S)"
DO_BATCH=false
# Render-only by default: verification is decoupled from dashboard rendering.
# --raw (including legacy --vc71 aliases) measures bytes; --mnemonic selects optional analysis.
DO_VC71=false
VC71_MODE="--incremental"
DO_RAW=false
RAW_WORKERS=4

while [[ $# -gt 0 ]]; do
    case "$1" in
        --batch)     DO_BATCH=true; shift ;;
        --vc71|--full-vc71) DO_RAW=true; shift ;;
        --mnemonic)  DO_VC71=true; VC71_MODE="--incremental"; shift ;;
        --full-mnemonic) DO_VC71=true; VC71_MODE=""; shift ;;
        --skip-vc71) DO_VC71=false; shift ;;
        --raw)       DO_RAW=true; shift ;;
        --raw-workers) RAW_WORKERS="$2"; shift 2 ;;
        --run-id)    RUN_ID="$2"; shift 2 ;;
        --help|-h)   usage ;;
        *)           echo "Unknown: $1"; usage ;;
    esac
done

cd "$(git rev-parse --show-toplevel 2>/dev/null || echo .)"

if $DO_BATCH; then
    echo "=== Running batch equivalence tests ==="
    $VENV tools/equivalence/batch_verify.py \
        --seeds 50 --timeout 120 --csv --skip-existing \
        --allowlist tools/equivalence/batch_verify_allowlist.json \
        --output-dir artifacts/batch_verify
fi

if $DO_VC71; then
    if [[ -n "$VC71_MODE" ]]; then
        echo "=== Refreshing mnemonic similarity (NOT raw byte accuracy) (incremental) ==="
    else
        echo "=== Refreshing mnemonic similarity (NOT raw byte accuracy) (full) ==="
    fi
    $VENV tools/verify/vc71_regression.py populate $VC71_MODE
fi

if $DO_RAW; then
    echo "=== Refreshing raw-XBE byte accuracy ==="
    RAW_OUTPUT="artifacts/byte_measurements/$RUN_ID-$(date +%s%N)"
    mkdir -p "$RAW_OUTPUT"
    printf '{"sources":null}\n' > "$RAW_OUTPUT/plan.json"
    $VENV tools/verify/byte_regression.py measure \
        --commit "$(git rev-parse HEAD)" --run-id "$RUN_ID" \
        --plan "$RAW_OUTPUT/plan.json" --output "$RAW_OUTPUT/snapshot.json" \
        --workers "$RAW_WORKERS" --allow-dirty --publish
fi

echo "=== Generating CI status page ==="
mkdir -p artifacts/progress
$VENV tools/report/generate_ci_status.py --output-dir artifacts/progress

echo "=== Generating dashboard ==="
$VENV tools/report/generate_decomp_report.py \
    --output artifacts/progress/report.json \
    --html artifacts/progress/index.html

echo "=== Recording snapshot ($RUN_ID) ==="
$VENV tools/report/history.py \
    --add-snapshot artifacts/progress/report.json

echo "=== Done ==="
echo "Open http://localhost:8080/ in your browser"
echo "Or run: python3 tools/report/progress_server.py"
