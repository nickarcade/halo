#!/usr/bin/env python3
"""Synthetic soundness tests for the raw-XBE structural lane."""

import importlib.util
import struct
import tempfile
import pathlib
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("raw_xbe_structural", HERE / "raw_xbe_structural.py")
raw = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(raw)


def relocation(offset, reloc_type=raw.IMAGE_REL_I386_REL32, symbol="FUN_00001010"):
    return {"offset": offset, "absolute_offset": offset, "symbol_index": 0,
            "type": reloc_type, "type_name": raw.RELOCATION_NAMES.get(reloc_type, "UNKNOWN"),
            "symbol": {"name": symbol}}


def resolved(target=0x1020):
    return {target: {"logical_target": target}}


class TestStructuralComparison(unittest.TestCase):
    def test_rel32_operand_is_masked_but_non_relocation_bytes_are_literal(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2)], reference, 0x1000, resolved(0x1021))
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["confidence"], "high")
        self.assertTrue(result["relocations"][0]["comparable"])

    def test_rel32_linked_displacement_differs_but_logical_target_matches(self):
        # COFF candidate field A=0x10; linked raw displacement is S+A-P=0x1b.
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x10\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1)], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural exact")

    def test_target_identity_is_optional_evidence(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x11\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1, symbol="FUN_00001020")], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["identity_evidence"]["status"], "unresolved")
        self.assertFalse(result["identity_evidence"]["required_for_structural_match"])

    def test_injected_identity_mismatch_does_not_change_structural_verdict(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x10\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1)], reference, 0x1000,
                             {0x1020: {"logical_target": 0x9999}})
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["identity_evidence"]["status"], "unresolved")

    def test_metrics_report_only_unmasked_byte_accuracy(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2)], reference, 0x1000)
        self.assertEqual(result["matching_non_relocation_bytes"], 3)
        self.assertEqual(result["byte_counts"]["relocation_operand"], 4)
        self.assertEqual(result["byte_counts"]["non_relocation"], 3)
        self.assertEqual(result["byte_accuracy"], 1.0)
        self.assertTrue(result["relocation_shape_evidence"]["matched"])

    def test_shape_mismatch_refuses_masking(self):
        # The function still scores, but nothing is masked and the score is
        # flagged as a lower bound.  Refusing to mask can only lower it.
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(3)], reference, 0x1000)
        self.assertEqual(result["relocation_shape_evidence"]["status"], "mismatch")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])
        self.assertEqual(result["unmasked_relocations"], 1)

    def test_relocation_location_mismatch_is_not_exact(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(3)], reference, 0x1000, resolved())
        self.assertNotEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_relocation_type_mismatch_is_never_masked(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_relocation_count_mismatch_does_not_create_exactness(self):
        # Identical bytes, but the candidate declares a relocation the reference
        # cannot account for, so nothing may be masked on its behalf.
        reference = b"\x90\xc3"
        candidate = b"\x90\xc3"
        result = raw.compare(candidate, [relocation(0)], reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])
        self.assertEqual(result["unmasked_relocations"], 1)

    def test_relocation_not_on_an_operand_field_is_never_masked(self):
        # Offset 2 is inside the jcc opcode, not its displacement field.
        reference = b"\x90\x0f\x80\x00\x00\x00\x00\xc3"
        candidate = b"\x90\x0f\x80\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2)], reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_jcc_rel32_displacement_is_supported(self):
        # A near-jcc displacement is as unambiguous as a call's; the earlier
        # opcode whitelist simply did not list it.
        reference = b"\x90\x0f\x80\x00\x00\x00\x00\xc3"
        candidate = b"\x90\x0f\x80\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(3)], reference, 0x1000, resolved())
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 4)

    def test_push_imm32_absolute_is_supported(self):
        reference = b"\x68\x20\x10\x00\x00\xc3"
        candidate = b"\x68\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, resolved())
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 4)

    def test_dir32_relocation_on_a_branch_field_is_refused(self):
        # A DIR32 must never be masked over a relative branch displacement.
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_size_mismatch_counts_against_byte_accuracy(self):
        # A truncated candidate whose prefix matches must not read as 100%.
        reference = b"\x90\x90\x90\x90\xc3"
        candidate = b"\x90\x90"
        result = raw.compare(candidate, [], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(result["byte_counts"]["non_relocation"], 5)
        self.assertEqual(result["matching_non_relocation_bytes"], 2)
        self.assertAlmostEqual(result["byte_accuracy"], 2 / 5)

    def test_aligned_bytes_resynchronize_after_shorter_instruction(self):
        reference = b"\x81\xec\xd0\x00\x00\x00\x53\xc3"
        candidate = b"\x83\xec\x68\x53\xc3"
        result = raw.compare(candidate, [], reference, 0x1000)
        aligned = result["aligned_byte_match"]
        self.assertEqual(aligned["status"], "scored")
        self.assertEqual(aligned["matching_bytes"], 3)
        self.assertEqual(aligned["compared_bytes"], 8)
        self.assertAlmostEqual(aligned["byte_accuracy"], 3 / 8)
        self.assertGreater(aligned["byte_accuracy"], result["byte_accuracy"])

    def test_aligned_bytes_penalize_insert_and_resynchronize(self):
        reference = b"\x53\xc3"
        candidate = b"\x90\x53\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 2)
        self.assertEqual(aligned["compared_bytes"], 3)
        self.assertFalse(aligned["accuracy_is_lower_bound"])
        self.assertEqual(aligned["candidate_only_instructions"], 1)

    def test_dp_alignment_disambiguates_repeated_push_run(self):
        reference = b"\x51\x50\x53\xc3"
        candidate = b"\x50\x53\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["method"], "weighted_global_dp_v1")
        self.assertEqual(aligned["matching_bytes"], 3)
        self.assertEqual(aligned["compared_bytes"], 4)
        self.assertEqual(aligned["reference_only_instructions"], 1)
        self.assertEqual(aligned["normalized_exact_instructions"], 3)

    def test_dp_alignment_disambiguates_repeated_mov_run(self):
        reference = b"\x89\xc9\x89\xc0\x89\xdb\xc3"
        candidate = b"\x89\xc0\x89\xdb\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 5)
        self.assertEqual(aligned["compared_bytes"], 7)
        self.assertEqual(aligned["reference_only_instructions"], 1)
        self.assertEqual(aligned["normalized_exact_instructions"], 3)

    def test_aligned_bytes_keep_wrong_immediate_visible(self):
        reference = b"\x68\x01\x00\x00\x00\xc3"
        candidate = b"\x68\x02\x00\x00\x00\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 5)
        self.assertEqual(aligned["compared_bytes"], 6)

    def test_aligned_relocation_masks_at_independent_instruction_offsets(self):
        reference = b"\x90\xe8\x1a\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate, [relocation(1, symbol="FUN_00001020")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["resolved_relocations"], 1)
        self.assertEqual(aligned["masked_relocation_bytes"], 4)
        self.assertEqual(aligned["matching_bytes"], 2)
        self.assertEqual(aligned["compared_bytes"], 3)

    def test_aligned_relocation_masks_at_independent_operand_offsets(self):
        reference = b"\x8b\x05\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate,
            [relocation(1, raw.IMAGE_REL_I386_DIR32, "FUN_00001020")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["resolved_relocations"], 1)
        self.assertEqual(aligned["masked_relocation_bytes"], 4)
        self.assertEqual(aligned["matching_bytes"], 1)
        self.assertEqual(aligned["compared_bytes"], 3)

    def test_aligned_relocation_does_not_mask_unresolved_target(self):
        reference = b"\x90\xe8\x1a\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate, [relocation(1, symbol="unknown_target")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["resolved_relocations"], 0)
        self.assertEqual(aligned["masked_relocation_bytes"], 0)
        self.assertEqual(aligned["unresolved_relocations"], 1)
        self.assertTrue(aligned["accuracy_is_lower_bound"])
        self.assertEqual(aligned["uncertain_relocation_bytes"], 4)
        self.assertAlmostEqual(aligned["byte_accuracy"], 2 / 7)
        self.assertAlmostEqual(aligned["byte_accuracy_upper_bound"], 6 / 7)

    def test_exact_ordinary_bytes_with_unknown_relocation_report_range(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate, [relocation(1, symbol="unknown_target")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 2)
        self.assertEqual(aligned["compared_bytes"], 6)
        self.assertAlmostEqual(aligned["byte_accuracy"], 1 / 3)
        self.assertEqual(aligned["byte_accuracy_upper_bound"], 1.0)

    def test_reference_only_site_does_not_veto_scoring(self):
        # The raw-XBE sweep sees an absolute operand the candidate has no
        # relocation for; that is a sweep artifact, so the function still scores.
        reference = b"\xa1\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\x20\x10\x00\x00\xc3"
        result = raw.compare(candidate, [], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertEqual(result["byte_accuracy"], 1.0)

    def test_dir32_unambiguous_absolute_mov_is_supported(self):
        reference = b"\xa1\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, {0x1020: {"logical_target": 0x1020}})
        self.assertEqual(result["verdict"], "structural exact")

    def test_summary_reports_function_and_byte_weighted_metrics(self):
        records = [{
            "schema_version": 1, "lane": "raw_xbe_structural", "verdict": "structural exact",
            "address": "0x00001000", "function": "exact",
            "reference": {"length": 8}, "matching_non_relocation_bytes": 4,
            "byte_counts": {"non_relocation": 4, "relocation_operand": 4},
            "aligned_byte_match": {
                "status": "scored", "matching_bytes": 6, "compared_bytes": 8,
                "uncertain_relocation_bytes": 2, "accuracy_is_provisional": False,
            },
        }, {
            "schema_version": 1, "lane": "raw_xbe_structural", "verdict": "structural differ",
            "address": "0x00001008", "function": "differ",
            "reference": {"length": 4}, "matching_non_relocation_bytes": 2,
            "byte_counts": {"non_relocation": 4, "relocation_operand": 0},
            "aligned_byte_match": {
                "status": "scored", "matching_bytes": 2, "compared_bytes": 4,
                "uncertain_relocation_bytes": 0, "accuracy_is_provisional": True,
            },
        }]
        old_extent = raw.xref.function_extent
        old_xbe = raw.xref.XBE
        raw.xref.function_extent = lambda address: (address + 4, "table", "table")
        try:
            with tempfile.TemporaryDirectory() as temp:
                raw.ROOT = Path(temp)
                raw.xref.XBE = Path(temp) / "cachebeta.xbe"
                raw.xref.XBE.write_bytes(b"xbe")
                summary = raw._write_summary(records, ["populate", "--workers", "2"], 2, 0)
        finally:
            raw.xref.function_extent = old_extent
            raw.xref.XBE = old_xbe
        self.assertEqual(summary["schema_version"], 1)
        self.assertEqual(summary["lane"], "raw_xbe_structural")
        self.assertEqual(summary["function_totals"]["structural_exact"], 1)
        self.assertEqual(summary["byte_totals"]["matching_bytes"], 6)
        self.assertEqual(summary["byte_totals"]["masked_bytes"], 4)
        self.assertEqual(summary["byte_totals"]["byte_accuracy"], 0.75)
        self.assertEqual(summary["byte_totals"]["masked_accuracy"], 10 / 12)
        self.assertEqual(summary["aligned_byte_totals"]["scored_functions"], 2)
        self.assertEqual(summary["aligned_byte_totals"]["matching_bytes"], 8)
        self.assertEqual(summary["aligned_byte_totals"]["compared_bytes"], 12)
        self.assertEqual(summary["aligned_byte_totals"]["byte_accuracy_lower"], 8 / 12)
        self.assertEqual(summary["aligned_byte_totals"]["byte_accuracy_upper"], 10 / 12)
        self.assertEqual(summary["aligned_byte_totals"]["provisional_functions"], 1)

    def test_coff_relocation_parse_preserves_type_and_symbol(self):
        code = b"\xe8\x00\x00\x00\x00\xc3"
        text_offset = 60
        reloc_offset = text_offset + len(code)
        symbol_offset = reloc_offset + 10
        header = struct.pack("<HHIIIHH", 0x14C, 1, 0, symbol_offset, 3, 0, 0)
        section = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, len(code), text_offset,
                              reloc_offset, 0, 1, 0, 0x60000020)
        reloc = struct.pack("<IIH", 1, 2, raw.IMAGE_REL_I386_REL32)
        target = struct.pack("<8sIhHBB", b"target\0\0", 0, 0, 0, 2, 0)
        function = struct.pack("<8sIhHBB", b"function", 0, 1, raw.IMAGE_SYM_DTYPE_FUNCTION, 2, 1)
        aux = struct.pack("<IIIIH", 0, len(code), 0, 0, 0)
        image = header + section + code + reloc + function + aux + target + struct.pack("<I", 13) + b"function\0"
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "candidate.obj"
            path.write_bytes(image)
            parsed, relocs, _ = raw._parse_coff(path, "function")
        self.assertEqual(parsed, code)
        self.assertEqual(relocs[0]["type_name"], "REL32")
        self.assertEqual(relocs[0]["symbol"]["name"], "target")


if __name__ == "__main__":
    unittest.main()


class ReferenceProvenanceTest(unittest.TestCase):
    """The reference hash must identify the pristine XBE file, not the span.

    Recording the function's own span hash there looks plausible and is what the
    lane originally did, but the report validates it against the XBE file hash,
    so every record silently failed validation and the dashboard column stayed
    empty.  Both audit lanes must agree on what this field means.
    """

    def test_reference_sha256_is_the_xbe_file_hash(self):
        import raw_byte_audit as strict
        import xbe_reference as xref
        if not pathlib.Path(xref.XBE).is_file():
            self.skipTest("pristine XBE is not available")
        expected = strict.sha256_file(xref.XBE)
        self.assertEqual(raw._hash_path(xref.XBE), expected)

    def test_span_hash_is_not_used_as_reference_provenance(self):
        import xbe_reference as xref
        if not pathlib.Path(xref.XBE).is_file():
            self.skipTest("pristine XBE is not available")
        span = raw._sha256(b"\x90\xc3")
        self.assertNotEqual(raw._hash_path(xref.XBE), span)


class RegisterArgumentFlagTest(unittest.TestCase):
    """Register-argument functions are flagged, and still scored.

    They ARE scored: the patched binary genuinely does not reproduce the
    original's bytes for them, so excluding them would report our accuracy as
    higher than it is.  The flag records why the number is low.
    """

    def setUp(self):
        raw._REG_ARG_CACHE.clear()

    def tearDown(self):
        raw._REG_ARG_CACHE.clear()

    def test_reg_arg_addresses_come_from_kb_decls(self):
        addresses = raw._register_argument_addresses()
        self.assertIsInstance(addresses, set)
        # The repository has a substantial @<reg> population; an empty set would
        # silently disable the exclusion.
        self.assertGreater(len(addresses), 0)

    def test_every_reg_arg_address_declares_a_register(self):
        import json
        addresses = raw._register_argument_addresses()
        kb = json.loads((raw.ROOT / "kb.json").read_text(encoding="utf-8"))
        decls = {}
        for obj in kb.get("objects", []):
            for fn in obj.get("functions", []):
                if fn.get("addr"):
                    decls[int(fn["addr"], 0)] = fn.get("decl") or ""
        for address in list(addresses)[:50]:
            if address in decls:
                self.assertIn("@<", decls[address])
