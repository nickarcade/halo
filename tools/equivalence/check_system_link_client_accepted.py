"""Compare 2276 client-acceptance paths with the active C body."""

import contextlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import abi

# Client state and game machines extend beyond the generic pointer slot.
abi.POINTER_SLOT = 0x1000
os.environ["BIPED_SIBLING_RESOLVE"] = "1"
import unicorn_diff


CASES = (
    ("index1", 1, "0x00124ff5", "0x0012503d"),
    ("index3", 3, "0x00124ff5", "0x0012503d"),
    ("create_fail", 1, "0x0012502a", "0x00124ff5"),
    ("write_fail", 1, "0x00125017", None),
    ("index4", None, "0x00124f94", "0x00124ff5"),
    ("index_negative", None, "0x0012503d", "0x00124ff5"),
)
SNAPSHOTS = Path(__file__).parent / "regression_snapshots"
original_run = unicorn_diff._run_function
states = []


def capture_run(*args, **kwargs):
    result = original_run(*args, **kwargs)
    states.append(result)
    return result


unicorn_diff._run_function = capture_run

for name, machine_index, required_pc, forbidden_pc in CASES:
    states.clear()
    with tempfile.TemporaryDirectory() as temporary:
        report_path = Path(temporary) / "result.json"
        sys.argv = [
            "unicorn_diff.py", "network_game_client_accepted_into_game",
            "--allow-stubs", "--no-real-callees", "--seeds", "1",
            "--state-snapshot", str(SNAPSHOTS / f"system_link_client_accepted_{name}.json"),
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
    for state in states:
        data = state.scratch_data
        expected_index = 0xffff if machine_index is None else machine_index
        expected_state = 1 if machine_index is None else 2
        if (int.from_bytes(data[:2], "little") != expected_index or
                int.from_bytes(data[0xca6:0xca8], "little") != expected_state):
            raise AssertionError((name, "client index or state differs"))
        if machine_index is not None and data[0x9b0 + machine_index * 0x44] != machine_index:
            raise AssertionError((name, "selected machine slot differs"))
    print(name, "matches", report["coverage_pct"], "% oracle coverage")
