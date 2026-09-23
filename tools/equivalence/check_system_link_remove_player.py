"""Compare client player-removal branches with the pristine 2276 function."""

import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import abi

# Client state is at +0xca6, beyond the generic pointer slot.
abi.POINTER_SLOT = 0x1000
import unicorn_diff


CASES = (
    ("pregame", 1, "0x001264ad", "0x0012648d"),
    ("ingame", 1, "0x0012648d", None),
    ("postgame_write_fail", 0, "0x00126500", None),
    ("pregame_create_fail", 0, "0x00126475", "0x001264ad"),
    ("searching", 0, "0x0012643d", "0x001264ad"),
)
SNAPSHOTS = Path(__file__).parent / "regression_snapshots"
original_run = unicorn_diff._run_function
states = []


def capture_run(*args, **kwargs):
    result = original_run(*args, **kwargs)
    states.append(result)
    return result


unicorn_diff._run_function = capture_run

for name, expected_return, required_pc, forbidden_pc in CASES:
    states.clear()
    with tempfile.TemporaryDirectory() as temporary:
        report_path = Path(temporary) / "result.json"
        sys.argv = [
            "unicorn_diff.py", "network_game_client_request_remove_player",
            "--allow-stubs", "--no-real-callees", "--seeds", "1",
            "--state-snapshot", str(SNAPSHOTS / f"system_link_remove_player_{name}.json"),
            "--no-concolic", "--no-leaf-cache", "--output-json", str(report_path), "-q",
        ]
        with contextlib.redirect_stdout(io.StringIO()):
            try:
                result = unicorn_diff.main()
            except SystemExit as exc:
                result = exc.code
        report = json.loads(report_path.read_text())
    pcs = report["covered_pcs"]
    if (result or report["passed"] != 1 or report["failed"] or report["errors"] or
            required_pc not in pcs or (forbidden_pc and forbidden_pc in pcs) or
            len(states) != 2 or any(state.error for state in states) or
            states[0].scratch_data != states[1].scratch_data or
            any((state.eax & 0xff) != expected_return for state in states)):
        raise AssertionError((name, "oracle and candidate differ or wrong path", report))
    print(name, "matches", report["coverage_pct"], "% oracle coverage")
