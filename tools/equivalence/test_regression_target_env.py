#!/usr/bin/env python3
"""Regression runner test for per-target child-process environment overrides."""

import contextlib
import io
import os
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))

import regression_test


class TestRegressionTargetEnvironment(unittest.TestCase):
    def test_target_env_merges_without_dropping_parent_variables(self):
        calls = []

        def fake_run(command, **kwargs):
            calls.append((command, kwargs))
            if command[0] == "git":
                return SimpleNamespace(stdout="", stderr="", returncode=0)
            return SimpleNamespace(
                stdout="RESULTS: 1 seeds, 0 failed, 0 errors\n",
                stderr="",
                returncode=0,
            )

        target = {
            "name": "synthetic_env_target",
            "seeds": 1,
            "env": {
                "REGRESSION_TARGET_ONLY": "target-value",
            },
        }
        with mock.patch.dict(
                regression_test.os.environ,
                {"REGRESSION_PARENT_SENTINEL": "parent-value"},
                clear=True):
            with mock.patch.object(
                    regression_test.subprocess, "run", side_effect=fake_run):
                status, _detail = regression_test.run_target(target)

        self.assertEqual(status, "pass")
        child_env = calls[-1][1]["env"]
        self.assertEqual(child_env["REGRESSION_PARENT_SENTINEL"], "parent-value")
        self.assertEqual(child_env["REGRESSION_TARGET_ONLY"], "target-value")


class TestRegressionParallelism(unittest.TestCase):
    def test_parallel_results_keep_target_order_and_isolate_leaf_cache(self):
        targets = [
            {"name": "slow", "addr": "0x1", "obj": "a.obj", "seeds": 1},
            {"name": "fast", "addr": "0x2", "obj": "b.obj", "seeds": 1},
        ]
        calls = []

        def fake_run(command, **kwargs):
            if command[0] == "git":
                return SimpleNamespace(stdout="", stderr="", returncode=0)
            calls.append((command, kwargs))
            if "slow" in command:
                import time
                time.sleep(0.03)
            return SimpleNamespace(
                stdout="RESULTS: 1 seeds, 0 failed, 0 errors\n",
                stderr="", returncode=0,
            )

        with mock.patch.object(regression_test, "load_targets", return_value=targets), \
                mock.patch.object(regression_test, "check_prerequisites", return_value=None), \
                mock.patch.object(regression_test.subprocess, "run", side_effect=fake_run), \
                mock.patch.object(regression_test.sys, "argv", ["regression_test.py", "-j", "2"]):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = regression_test.main()

        self.assertEqual(status, 0)
        self.assertLess(output.getvalue().index("slow"), output.getvalue().index("fast"))
        self.assertEqual(len(calls), 2)
        for command, _kwargs in calls:
            self.assertIn("--no-leaf-cache", command)

    def test_jobs_must_be_positive(self):
        with mock.patch.object(regression_test.sys, "argv", ["regression_test.py", "-j", "0"]):
            with self.assertRaises(SystemExit) as raised:
                regression_test.main()
        self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
