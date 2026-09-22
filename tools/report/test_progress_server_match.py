"""Regression test for the dashboard's score-button match totals."""

import unittest

from progress_server import _recompute_summary_match, _recompute_unit_match


class TestScoreButtonMatch(unittest.TestCase):
    def test_unported_scores_do_not_change_match_totals(self):
        unit = {
            'functions': [
                {'ported': True, 'match_percent': 90.0, 'size': 100},
                {'ported': False, 'match_percent': 1.0, 'size': 900},
            ],
            'summary': {},
        }
        report = {'units': [unit], 'summary': {}}

        _recompute_unit_match(unit)
        _recompute_summary_match(report)

        self.assertEqual(unit['summary']['match_avg'], 90.0)
        self.assertEqual(unit['summary']['match_weighted'], 90.0)
        self.assertEqual(report['summary']['match'], {
            'average': 90.0, 'weighted': 90.0, 'scored_count': 1,
        })


if __name__ == '__main__':
    unittest.main()
