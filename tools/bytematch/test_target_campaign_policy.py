#!/usr/bin/env python3
"""Unit tests for campaign queue policy (no compiler or XBE)."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import target_campaign as campaign  # noqa: E402


class AttemptPolicyTest(unittest.TestCase):
    def test_recent_no_gain_is_skipped(self):
        attempt = {"result": "no_gain", "run": "run-2"}
        self.assertTrue(campaign.skip_attempt(attempt, {"run-1", "run-2"}))

    def test_old_no_gain_is_retried(self):
        attempt = {"result": "no_gain", "run": "run-0"}
        self.assertFalse(campaign.skip_attempt(attempt, {"run-1", "run-2"}))

    def test_legacy_no_gain_is_retried(self):
        self.assertFalse(campaign.skip_attempt({"result": "no_gain"}, {"run-1", "run-2"}))

    def test_ceiling_stays_parked_while_dependencies_match(self):
        attempt = {"result": "regarg_ceiling", "dependency_fingerprint": "same"}
        self.assertTrue(campaign.skip_attempt(attempt, set(), "same"))

    def test_dependency_change_reopens_ceiling(self):
        attempt = {"result": "regarg_ceiling", "dependency_fingerprint": "old"}
        self.assertFalse(campaign.skip_attempt(attempt, set(), "new"))


if __name__ == "__main__":
    unittest.main()
