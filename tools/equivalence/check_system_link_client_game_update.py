"""Compare valid client game-update branches with the pristine 2276 body."""

import contextlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import abi

# The client update sequence is at +0xc98; both pointer slots must fit it.
abi.POINTER_SLOT = 0x1000
os.environ["BIPED_SIBLING_RESOLVE"] = "1"
import unicorn_diff


CASES = (
    ("match", "0x001254f8", "0x001253fe", 1),
    ("missed", "0x001253fe", None, 1),
    ("local_time", "0x00125438", None, 1),
    ("local_seed", "0x0012545c", None, 1),
    ("count_expand", "0x001253c9", None, 2),
)
SNAPSHOTS = Path(__file__).parent / "regression_snapshots"
original_run = unicorn_diff._run_function
states = []


def capture_run(*args, **kwargs):
    result = original_run(*args, **kwargs)
    states.append(result)
    return result


unicorn_diff._run_function = capture_run

for name, required_pc, forbidden_pc, expected_count in CASES:
    states.clear()
    with tempfile.TemporaryDirectory() as temporary:
        report_path = Path(temporary) / "result.json"
        sys.argv = [
            "unicorn_diff.py", "network_game_client_handle_game_update",
            "--allow-stubs", "--no-real-callees", "--seeds", "1",
            "--state-snapshot", str(SNAPSHOTS / f"system_link_client_game_update_{name}.json"),
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
        if ((state.eax & 0xff) != 1 or
                int.from_bytes(data[0xc98:0xc9c], "little") != 2 or
                int.from_bytes(data[0xc9c:0xca0], "little") != 1234 or
                int.from_bytes(data[0x100e:0x1010], "little") != expected_count):
            raise AssertionError((name, "expected update state was not written"))
    print(name, "matches", report["coverage_pct"], "% oracle coverage")
