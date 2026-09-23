"""Compare three reachable 2276 client-joining paths with the active C body."""

import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import abi

# The join-sent flag is at client+0xcaa, beyond the generic 0x400-byte slot.
abi.POINTER_SLOT = 0x1000
import unicorn_diff


CASES = (
    ("connected", "0x00126c3a", "0x00126bbf", None),
    ("create_fail", "0x00126bbf", None, None),
    ("timeout", "0x00126c80", None, b"\0\0\0\0"),
)
SNAPSHOTS = Path(__file__).parent / "regression_snapshots"
original_run = unicorn_diff._run_function
states = []


def capture_run(*args, **kwargs):
    result = original_run(*args, **kwargs)
    states.append(result)
    return result


unicorn_diff._run_function = capture_run

for name, required_pc, forbidden_pc, expected_handle in CASES:
    states.clear()
    with tempfile.TemporaryDirectory() as temporary:
        report_path = Path(temporary) / "result.json"
        sys.argv = [
            "unicorn_diff.py", "network_game_client_idle_joining",
            "--allow-stubs", "--no-real-callees", "--seeds", "1",
            "--state-snapshot", str(SNAPSHOTS / f"system_link_client_joining_{name}.json"),
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
            states[0].scratch_data != states[1].scratch_data):
        raise AssertionError((name, "oracle and candidate differ or wrong path", report))
    if expected_handle is not None:
        for state in states:
            if state.scratch_data[0x830:0x834] != expected_handle:
                raise AssertionError((name, "connect handle was not cleared"))
    print(name, "matches", report["coverage_pct"], "% oracle coverage")
