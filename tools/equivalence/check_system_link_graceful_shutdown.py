"""Compare the 2276 graceful-shutdown branches with the ported C."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SNAPSHOTS = ROOT / "tools/equivalence/regression_snapshots"
CASES = {
    "pregame_send": ("0x0012c496", "0x0012c4d1"),
    "postgame_send": ("0x0012c466", "0x0012c4d1"),
    "pregame_create_fail": ("0x0012c496", "0x0012c4ad"),
    "postgame_create_fail": ("0x0012c466", "0x0012c483"),
    "pregame_send_fail": ("0x0012c496", "0x0012c4e4"),
    "inactive": ("0x0012c4f1", "0x0012c4f6"),
}


for name, required_pcs in CASES.items():
    with tempfile.TemporaryDirectory() as temporary:
        report_path = Path(temporary) / "result.json"
        command = [
            sys.executable, "tools/equivalence/unicorn_diff.py",
            "network_game_server_graceful_shutdown", "--allow-stubs",
            "--no-real-callees", "--seeds", "1", "--state-snapshot",
            str(SNAPSHOTS / f"system_link_graceful_shutdown_{name}.json"),
            "--no-concolic", "--no-leaf-cache", "--output-json",
            str(report_path), "-q",
        ]
        environment = dict(os.environ, BIPED_SIBLING_RESOLVE="1")
        result = subprocess.run(command, cwd=ROOT, env=environment, check=False,
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError((name, result.stdout, result.stderr))
        report = json.loads(report_path.read_text())
    if (report["passed"] != 1 or report["failed"] or report["errors"] or
            not all(pc in report["covered_pcs"] for pc in required_pcs)):
        raise AssertionError((name, "oracle path or candidate differs", report))
    print(name, "matches", report["coverage_pct"], "% oracle coverage")
