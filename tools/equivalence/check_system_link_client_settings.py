"""Compare 2276 client settings-update branches with the ported C."""

import contextlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/equivalence"))
import abi

abi.POINTER_SLOT = 0x1000
os.environ["BIPED_SIBLING_RESOLVE"] = "1"
import unicorn_diff


CASES = {
    "bad_source": "0x001275ee",
    "wrong_state": "0x001275d9",
    "decode_fail": "0x001275c4",
    "update_rejected": "0x001275af",
    "update_applied": "0x001275a1",
}
SNAPSHOTS = ROOT / "tools/equivalence/regression_snapshots"

for name, required_pc in CASES.items():
    with tempfile.TemporaryDirectory() as temporary:
        report_path = Path(temporary) / "result.json"
        sys.argv = [
            "unicorn_diff.py",
            "network_game_client_handle_message_server_game_settings_update",
            "--allow-stubs", "--no-real-callees", "--seeds", "1",
            "--state-snapshot",
            str(SNAPSHOTS / f"system_link_client_settings_{name}.json"),
            "--no-concolic", "--no-leaf-cache",
            # The harness intercepts the same-TU address helper differently
            # on the two sides; its transitive helper-call trace is asymmetric.
            "--no-stub-arg-trace", "--output-json", str(report_path), "-q",
        ]
        with contextlib.redirect_stdout(io.StringIO()):
            try:
                result = unicorn_diff.main()
            except SystemExit as exc:
                result = exc.code
        report = json.loads(report_path.read_text())
    if (result or report["passed"] != 1 or report["failed"] or report["errors"] or
            required_pc not in report["covered_pcs"]):
        raise AssertionError((name, "oracle path or candidate differs", report))
    print(name, "matches", report["coverage_pct"], "% oracle coverage")
