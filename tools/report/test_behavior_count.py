import unittest

from tools.report.check_behavior_count import behavioral_matches


class BehaviorCountTests(unittest.TestCase):
    def test_count_matches_dashboard_rules(self):
        report = {"units": [
            {"synthetic": True, "functions": [{"ported": True,
                                                "snapshot_passed": True}]},
            {"functions": [
                {"ported": True, "equiv_status": "pass",
                 "equiv_confidence": "high"},
                {"ported": True, "equiv_status": "pass",
                 "equiv_proven": True},
                {"ported": True, "snapshot_passed": True,
                 "runtime_oracle_passed": True},
                {"ported": True, "equiv_status": "fail",
                 "equiv_reason": "divergence", "equiv_confidence": "moderate",
                 "snapshot_passed": True},
                {"ported": True, "equiv_status": "pass",
                 "equiv_confidence": "weak"},
            ]},
        ]}
        self.assertEqual(behavioral_matches(report), 3)


if __name__ == "__main__":
    unittest.main()
