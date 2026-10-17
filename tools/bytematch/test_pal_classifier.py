#!/usr/bin/env python3
"""Unit tests for deterministic raw-byte residual classification."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pal_campaign as campaign  # noqa: E402


def record(classes=None, abi="other_differences", **aligned):
    block = {"difference_classes": classes or {}}
    block.update(aligned)
    return {"aligned_byte_match": block,
            "register_argument_residual": {"status": abi}}


class ResidualClassifierTest(unittest.TestCase):
    def test_exact_except_register_abi_routes_out_of_shape_search(self):
        lane, probability = campaign.classify_residual(
            record({"candidate_only:stack_param_load": 1},
                   abi="exact_except_register_abi"))
        self.assertEqual((lane, probability), ("abi", 0.05))

    def test_uncertain_relocation_only_needs_resolver(self):
        lane, probability = campaign.classify_residual(
            record({}, uncertain_relocation_bytes=4, mismatched_relocations=0))
        self.assertEqual((lane, probability), ("relocation", 0.0))

    def test_missing_instructions_route_to_relift(self):
        lane, _ = campaign.classify_residual(
            record({"candidate_only:instruction": 4, "register": 1}))
        self.assertEqual(lane, "relift")

    def test_operand_and_offset_residuals_route_to_types(self):
        lane, _ = campaign.classify_residual(
            record({"operands": 3, "stack_offset": 2, "register": 1}))
        self.assertEqual(lane, "types")

    def test_register_residuals_route_to_frame(self):
        lane, _ = campaign.classify_residual(
            record({"register": 4, "operand_order": 1}))
        self.assertEqual(lane, "frame")

    def test_simple_codegen_residuals_route_to_shape(self):
        lane, _ = campaign.classify_residual(
            record({"operand_order": 2, "immediate": 1}))
        self.assertEqual(lane, "shape")

    def test_near_exact_shape_gets_exactness_bonus(self):
        near = campaign.queue_priority(10, "Matching", "shape", 0.65, 0.96)
        far = campaign.queue_priority(10, "Matching", "shape", 0.65, 0.94)
        self.assertGreater(near, far)


if __name__ == "__main__":
    unittest.main()
