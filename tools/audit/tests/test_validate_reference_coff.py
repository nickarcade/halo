#!/usr/bin/env python3
"""Synthetic fail-closed tests for reference COFF validation."""

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("validate_reference_coff", HERE / "validate_reference_coff.py")
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


def coff(code=b"\x55\xc3", *, symbols=None, relocations=(), reloc_offset=None,
         raw_size=None):
    """Build a small i386 COFF object with optional function aux records."""
    symbols = symbols or [("target", 0, len(code), True)]
    text_offset = 60
    stored_size = len(code) if raw_size is None else raw_size
    relocation_offset = text_offset + len(code) if reloc_offset is None else reloc_offset
    symbol_offset = text_offset + len(code) + 10 * len(relocations)
    records = []
    for name, value, size, with_aux in symbols:
        records.append(struct.pack("<8sIhHBB", name.encode().ljust(8, b"\0"), value,
                                   1, validator.FUNCTION, 2, 1 if with_aux else 0))
        if with_aux:
            aux = bytearray(18)
            struct.pack_into("<I", aux, 4, size)
            records.append(bytes(aux))
    symbol_data = b"".join(records)
    symbol_count = len(records)
    string_table = b"\x04\x00\x00\x00"
    header = struct.pack("<HHIIIHH", validator.I386, 1, 0, symbol_offset,
                         symbol_count, 0, 0)
    section = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, stored_size,
                          text_offset, relocation_offset if relocations else 0, 0,
                          len(relocations), 0, 0x60000020)
    reloc = b"".join(struct.pack("<IIH", offset, sym_index, typ)
                      for offset, sym_index, typ in relocations)
    return header + section + code + reloc + symbol_data + string_table


class TestParseCoff(unittest.TestCase):
    def test_rejects_truncated_section_data(self):
        image = coff(b"\xc3", raw_size=100)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.obj"
            path.write_bytes(image)
            with self.assertRaisesRegex(validator.InvalidReference, "raw data is truncated"):
                validator.parse_coff(path)

    def test_rejects_relocation_table_offset_outside_file(self):
        image = coff(b"\xc3", relocations=[(0, 0, 6)], reloc_offset=0x1000)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.obj"
            path.write_bytes(image)
            with self.assertRaisesRegex(validator.InvalidReference, "relocation table is truncated"):
                validator.parse_coff(path)

    def test_rejects_relocation_field_past_section_end(self):
        image = coff(b"\xc3", relocations=[(1, 0, 6)])
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.obj"
            path.write_bytes(image)
            with self.assertRaisesRegex(validator.InvalidReference, "points outside"):
                validator.parse_coff(path)

    def test_rejects_relocation_symbol_index_outside_table(self):
        image = coff(b"\x00\x00\x00\x00", relocations=[(0, 99, 6)])
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.obj"
            path.write_bytes(image)
            with self.assertRaisesRegex(validator.InvalidReference, "invalid symbol index"):
                validator.parse_coff(path)


class TestFunctionValidity(unittest.TestCase):
    def _validate(self, image, expected=b"\x55\xc3", name="target"):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "reference.obj"
            path.write_bytes(image)
            return validator.validate_functions(path, [{
                "name": name, "address": 0x1000, "bytes": expected,
                "bound_provenance": "function_bounds.json",
            }])

    def test_valid_reference_has_machine_readable_provenance(self):
        result = self._validate(coff())
        self.assertTrue(result["usable_for_strict_coff"])
        self.assertEqual(result["authority"], "raw_xbe")
        self.assertEqual(result["checks"], {"coff_structure": "valid", "relocations": "valid"})
        self.assertEqual(result["functions"][0]["bound_provenance"], "function_bounds.json")
        self.assertEqual(result["functions"][0]["coff_end"], 2)

    def test_normalized_encoding_difference_can_be_reconciled(self):
        # Equivalent x86 encodings have different bytes but the same normalized
        # instruction shape; raw-byte equality is not asserted here.
        result = self._validate(coff(b"\x31\xc0\xc3"), expected=b"\x33\xc0\xc3")
        self.assertTrue(result["usable_for_strict_coff"])
        self.assertEqual(result["functions"][0]["normalized_sha256"],
                         result["functions"][0]["raw_normalized_sha256"])

    def test_no_aux_extent_with_duplicate_start_fails_closed(self):
        image = coff(b"\xc3\xc3", symbols=[
            ("target", 0, 0, False), ("alias", 0, 0, False)])
        result = self._validate(image, expected=b"\xc3")
        self.assertFalse(result["usable_for_strict_coff"])
        self.assertIn("ambiguous", result["functions"][0]["reason"])

    def test_aux_extent_overlapping_next_function_fails_closed(self):
        image = coff(b"\xc3\xc3", symbols=[
            ("target", 0, 2, True), ("next", 1, 1, True)])
        result = self._validate(image)
        self.assertFalse(result["usable_for_strict_coff"])
        self.assertIn("overlaps", result["functions"][0]["reason"])

    def test_normalized_code_mismatch_fails_closed(self):
        result = self._validate(coff(b"\x90\xc3"), expected=b"\x55\xc3")
        self.assertFalse(result["usable_for_strict_coff"])
        self.assertEqual(result["functions"][0]["reason"],
                         "normalized_code_does_not_match_raw_xbe_bound")

    def test_ambiguous_function_extent_fails_closed(self):
        image = coff(b"\x55\xc3\xc3", symbols=[
            ("target", 0, 0, False), ("target", 2, 1, False)])
        result = self._validate(image)
        self.assertFalse(result["usable_for_strict_coff"])
        self.assertEqual(result["functions"][0]["reason"],
                         "missing_or_ambiguous_function_symbol")
        self.assertEqual(result["functions"][0]["symbol_matches"], 2)

    def test_no_function_bound_is_not_strict_valid(self):
        result = self._validate(coff(b"\xc3"), expected=b"\xc3\xc3")
        self.assertFalse(result["usable_for_strict_coff"])
        self.assertIn("normalized_code_does_not_match_raw_xbe_bound",
                      result["functions"][0]["reason"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
