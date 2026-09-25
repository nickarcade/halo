#!/usr/bin/env python3
"""Unit tests for bytematch.py (no compiler, no XBE)."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bytematch as b  # noqa: E402


def sites_for(body, function="f", crlf=False):
    text = "int g;\nint f(int a, int b, int c)\n{\n%s\n}\n" % body
    if crlf:
        text = text.replace("\n", "\r\n")
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "t.c"
        with open(path, "w", newline="") as handle:
            handle.write(text)
        sites, read = b.find_sites(path, function)
    return sites, read


def rewrite(body, kind):
    sites, text = sites_for(body)
    chosen = [site for site in sites if site["kind"] == kind]
    return [b.apply_site(text, site) for site in chosen]


class TransformTest(unittest.TestCase):
    def test_swap_commutative(self):
        out = rewrite("  return a == b;", "swap_commutative")
        self.assertEqual(len(out), 1)
        self.assertIn("return b == a;", out[0])

    def test_association_is_preserved_with_parens(self):
        out = rewrite("  return a + b + c;", "swap_commutative")
        self.assertIn("return c + (a + b);", "".join(out))
        self.assertIn("return b + a + c;", "".join(out))

    def test_flip_relational(self):
        out = rewrite("  return a < b;", "flip_relational")
        self.assertIn("return b > a;", out[0])
        out = rewrite("  return a >= b;", "flip_relational")
        self.assertIn("return b <= a;", out[0])

    def test_side_effects_are_never_moved(self):
        sites, _ = sites_for("  return g++ == b;")
        self.assertEqual([site for site in sites if site["kind"] == "swap_commutative"], [])
        sites, _ = sites_for("  return f(a, b, c) == f(b, a, c);")
        self.assertEqual([site for site in sites if site["kind"] == "swap_commutative"], [])

    def test_one_call_beside_a_pure_operand_may_move(self):
        out = rewrite("  return f(a, b, c) == b;", "swap_commutative")
        self.assertIn("return b == f(a, b, c);", "".join(out))

    def test_non_commutative_operator_is_not_swapped(self):
        sites, _ = sites_for("  return a - b;")
        self.assertEqual(sites, [])

    def test_invert_if(self):
        out = rewrite("  if (a) { g = 1; } else { g = 2; }\n  return 0;", "invert_if")
        self.assertIn("if (!(a)) { g = 2; } else { g = 1; }", out[0])

    def test_invert_if_refuses_assert_and_else_if(self):
        sites, _ = sites_for("  if (a) { assert_halt(b); } else { g = 2; }\n  return 0;")
        self.assertEqual([site for site in sites if site["kind"] == "invert_if"], [])
        sites, _ = sites_for("  if (a) { g = 1; } else if (b) { g = 2; }\n  return 0;")
        self.assertEqual([site for site in sites if site["kind"] == "invert_if"], [])

    def test_invert_if_refuses_unequal_branch_lengths(self):
        sites, _ = sites_for("  if (a) {\n    g = 1;\n  } else { g = 2; }\n  return 0;")
        self.assertEqual([site for site in sites if site["kind"] == "invert_if"], [])

    def test_crlf_text_is_kept(self):
        sites, text = sites_for("  return a == b;", crlf=True)
        out = b.apply_site(text, sites[0])
        self.assertEqual(out.count("\r\n"), text.count("\r\n"))

    def test_only_the_named_function_is_searched(self):
        sites, _ = sites_for("  return a == b;", function="missing")
        self.assertEqual(sites, [])

    def test_parens_rule(self):
        self.assertFalse(b._needs_parens("a->b[3].c"))
        self.assertFalse(b._needs_parens("(a + b)"))
        self.assertTrue(b._needs_parens("(a) + (b)"))
        self.assertTrue(b._needs_parens("*(float *)0x10"))
        self.assertFalse(b._needs_parens("*(float *)0x10", "UNARY_OPERATOR"))
        self.assertTrue(b._needs_parens("a ? b : c", "CONDITIONAL_OPERATOR"))

    def test_casts_and_derefs_move_without_parens(self):
        out = rewrite("  return *(short *)(a + 2) != (int)b;", "swap_commutative")
        self.assertIn("return (int)b != *(short *)(a + 2);", "".join(out))


class LedgerTest(unittest.TestCase):
    def test_rule_table_rates(self):
        rows = [{"transform": "swap_commutative", "causes": {"operand_order": 1}, "improved": True},
                {"transform": "swap_commutative", "causes": {"operand_order": 1}, "improved": False},
                {"transform": "invert_if", "causes": {"operand_order": 1}, "improved": False}]
        table = b.rule_table(rows)
        self.assertEqual(table[("operand_order", "swap_commutative")]["trials"], 2)
        self.assertAlmostEqual(table[("operand_order", "swap_commutative")]["rate"], 0.5)
        self.assertAlmostEqual(table[("operand_order", "invert_if")]["rate"], 1 / 3)

    def test_order_sites_prefers_measured_then_prior(self):
        sites = [{"kind": "invert_if", "start": 1}, {"kind": "swap_commutative", "start": 2}]
        ordered = b.order_sites(sites, {"operand_order": 1}, {})
        self.assertEqual(ordered[0]["kind"], "swap_commutative")
        table = {("operand_order", "invert_if"): {"rate": 0.9}}
        ordered = b.order_sites(sites, {"operand_order": 1}, table)
        self.assertEqual(ordered[0]["kind"], "invert_if")


class QueueTest(unittest.TestCase):
    def record(self, name, accuracy, classes, verdict="structural differ", reg=False):
        return {"function": name, "address": name, "verdict": verdict,
                "register_argument": reg, "source": {"path": "s.c"},
                "aligned_byte_match": {"status": "scored", "byte_accuracy": accuracy,
                                       "difference_classes": classes}}

    def test_queue_ranks_by_remaining_causes(self):
        records = {r["address"]: r for r in (
            self.record("many", 0.95, {"register": 5}),
            self.record("few", 0.91, {"operand_order": 1, "branch_target": 9}),
            self.record("low", 0.5, {"register": 1}),
            self.record("done", 1.0, {}, verdict="structural exact"),
            self.record("abi", 0.99, {"candidate_only:stack_param_load": 1}, reg=True))}
        rows = b.queue(records)
        self.assertEqual([row["function"] for row in rows], ["few", "many", "abi"])
        self.assertEqual(rows[0]["causes"], {"operand_order": 1})

    def test_score_prefers_exact(self):
        self.assertGreater(b._score({"status": "scored", "byte_accuracy": 1.0}),
                           b._score({"status": "scored", "byte_accuracy": 0.99,
                                     "normalized_exact_instructions": 99}))
        self.assertLess(b._score({"status": "compile_failed"}),
                        b._score({"status": "scored", "byte_accuracy": 0.0}))


if __name__ == "__main__":
    unittest.main()
