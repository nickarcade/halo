"""Guard the dashboard's embedded JavaScript against unterminated string literals.

The HTML dashboard renders every function row from one large inline <script>.
A single syntax error there kills the whole script, so the page still loads but
renders nothing.  The classic way to introduce one is writing '\\n' instead of
'\\\\n' inside the Python source that emits the JS: Python turns it into a real
newline and the surrounding JS string literal is left open.

The check therefore runs against the *rendered* template value, not the Python
source text -- in the source an escape is still two ordinary characters, so the
defect is invisible there.
"""

import ast
import os
import re
import unittest

REPORT_DIR = os.path.dirname(os.path.abspath(__file__))
GENERATOR = os.path.join(REPORT_DIR, 'generate_decomp_report.py')

# A JS single-quoted string that both opens and closes on one line.
_JS_STRING = re.compile(r"'(?:[^'\\\n]|\\.)*'")


def dashboard_templates(source: str):
    """Return the rendered string literals that carry the dashboard's <script>."""
    templates = []
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            if '<script>' in node.value:
                templates.append(node.value)
    return templates


def unterminated_js_strings(template: str):
    """Yield (line_number, text) for JS lines holding an unclosed string literal."""
    problems = []
    in_script = False
    for number, line in enumerate(template.split('\n'), 1):
        if '<script>' in line:
            in_script = True
            continue
        if '</script>' in line:
            in_script = False
            continue
        if not in_script:
            continue
        stripped = _JS_STRING.sub('', line)
        # Any // left after string removal starts a real comment, where an
        # apostrophe is prose rather than an open literal.
        comment = stripped.find('//')
        if comment != -1:
            stripped = stripped[:comment]
        # A lone quote survives substitution only when its string never closed.
        if stripped.count("'") % 2 == 1:
            problems.append((number, line.strip()))
    return problems


class DashboardJsSyntaxTest(unittest.TestCase):
    def setUp(self):
        with open(GENERATOR, encoding='utf-8') as handle:
            self.source = handle.read()

    def test_generator_holds_exactly_one_dashboard_template(self):
        self.assertEqual(len(dashboard_templates(self.source)), 1)

    def test_emitted_js_has_no_unterminated_string_literals(self):
        problems = []
        for template in dashboard_templates(self.source):
            problems.extend(unterminated_js_strings(template))
        self.assertEqual(
            problems, [],
            'unterminated JavaScript string literal(s) in the dashboard template; '
            'a literal newline inside a JS string breaks the entire dashboard:\n' +
            '\n'.join('  line %d: %s' % item for item in problems))

    def test_detector_catches_a_real_newline_inside_a_js_string(self):
        broken = "<script>\nvar tip = 'first half\nsecond half';\n</script>\n"
        self.assertEqual([line for line, _ in unterminated_js_strings(broken)], [2, 3])

    def test_detector_accepts_an_escaped_newline(self):
        fine = "<script>\nvar tip = 'first half\\nsecond half';\n</script>\n"
        self.assertEqual(unterminated_js_strings(fine), [])

    def test_detector_ignores_an_apostrophe_in_a_comment(self):
        prose = "<script>\n// derive a reference for this unit's span\n</script>\n"
        self.assertEqual(unterminated_js_strings(prose), [])

    def test_mnemonic_score_does_not_count_as_verified(self):
        template = dashboard_templates(self.source)[0]
        verified_section = template.split(
            'function countVerified()', 1)[1].split(
            'function renderSummary()', 1)[0]
        self.assertNotIn(
            'match_percent', verified_section,
            'Mnemonic similarity is not behavioral verification.')
        self.assertIn('equivVerified(f)', verified_section)
        self.assertIn('snapshot_passed === true', verified_section)
        self.assertIn('runtime_oracle_passed === true', verified_section)

    def test_dashboard_calls_the_evidence_behavioral_matches(self):
        template = dashboard_templates(self.source)[0]
        self.assertIn('Behavioral matches', template)
        self.assertIn('Behavioral match', template)
        self.assertIn('Behavioral Evidence', template)
        self.assertNotIn('Verified functions', template)
        self.assertNotIn('Verification Coverage', template)


if __name__ == '__main__':
    unittest.main()
