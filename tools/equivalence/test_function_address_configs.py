#!/usr/bin/env python3
"""Pins that test configuration identifies functions by original address.

WHY
---
Function names are derived from kb.json only for reports, so semantic renames
cannot make a row inert or select a different function.

Run:  python3 tools/equivalence/test_function_address_configs.py
"""
import json
import re
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_ROOT = _HERE.parent.parent

ALLOWLIST = _HERE / "batch_verify_allowlist.json"
REGRESSION_TARGETS = _HERE / "regression_targets.json"
RUNTIME_TARGETS = _ROOT / "tools" / "verify" / "runtime_oracle_targets.json"
KB_JSON = _ROOT / "kb.json"

_DECL_NAME = re.compile(r"([A-Za-z_]\w*)\s*\(")
_ADDRESS = re.compile(r"^0x[0-9a-f]+$")


def _kb_index():
    """Map canonical original addresses to declaration names."""
    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    by_addr = {}

    def walk(node):
        if isinstance(node, dict):
            addr, decl = node.get("addr"), node.get("decl")
            if isinstance(addr, str) and isinstance(decl, str):
                m = _DECL_NAME.search(decl)
                if m:
                    try:
                        a = hex(int(addr, 16))
                    except ValueError:
                        a = None
                    if a is not None:
                        by_addr[a] = m.group(1)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(kb)
    return by_addr


class TestAllowlistAddressesResolve(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.targets = json.loads(
            ALLOWLIST.read_text(encoding="utf-8"))["targets"]
        cls.by_addr = _kb_index()

    def test_every_row_uses_a_canonical_known_address(self):
        invalid = []
        for addr in sorted(self.targets):
            if not _ADDRESS.fullmatch(addr):
                invalid.append(addr + " is not a lowercase hexadecimal address")
                continue
            if hex(int(addr, 16)) != addr:
                invalid.append(addr + " is not canonical")
                continue
            if addr not in self.by_addr:
                invalid.append(addr + " is absent from kb.json")
        self.assertEqual(invalid, [])


class TestRegressionTargetAddressesResolve(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.targets = json.loads(
            REGRESSION_TARGETS.read_text(encoding="utf-8"))["targets"]
        cls.by_addr = _kb_index()

    def test_every_row_uses_a_canonical_known_address(self):
        invalid = []
        for row in self.targets:
            addr = row.get("addr", "")
            if not _ADDRESS.fullmatch(addr):
                invalid.append(repr(addr) + " is not a lowercase hexadecimal address")
                continue
            if hex(int(addr, 16)) != addr:
                invalid.append(addr + " is not canonical")
                continue
            if addr not in self.by_addr:
                invalid.append(addr + " is absent from kb.json")
            if "name" in row or "label" in row:
                invalid.append(addr + " stores a rename-sensitive display name")
        self.assertEqual(invalid, [])


class TestRuntimeOracleTargetAddressesResolve(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.targets = json.loads(
            RUNTIME_TARGETS.read_text(encoding="utf-8"))["targets"]
        cls.by_addr = _kb_index()

    def test_every_row_uses_a_canonical_known_address(self):
        invalid = []
        for row in self.targets:
            addr = row.get("addr", "")
            if not _ADDRESS.fullmatch(addr):
                invalid.append(repr(addr) + " is not a lowercase hexadecimal address")
                continue
            if hex(int(addr, 16)) != addr:
                invalid.append(addr + " is not canonical")
                continue
            if addr not in self.by_addr:
                invalid.append(addr + " is absent from kb.json")
            if "target" in row or "name" in row:
                invalid.append(addr + " stores a rename-sensitive target name")
        self.assertEqual(invalid, [])


class TestAllowlistRowShape(unittest.TestCase):
    """An excuse with no written reason cannot be reviewed, and an
    un-reviewable excuse is how a real failure gets muted for good."""

    @classmethod
    def setUpClass(cls):
        cls.doc = json.loads(ALLOWLIST.read_text(encoding="utf-8"))
        cls.targets = cls.doc["targets"]

    def test_every_row_carries_statuses_and_reasons(self):
        for name, row in sorted(self.targets.items()):
            with self.subTest(target=name):
                self.assertIsInstance(row, dict)
                self.assertIsInstance(row.get("statuses"), list)
                self.assertTrue(row["statuses"])
                self.assertIsInstance(row.get("reasons"), list)
                self.assertTrue(row["reasons"])

    def test_every_reason_says_something(self):
        for name, row in sorted(self.targets.items()):
            for reason in row.get("reasons", []):
                with self.subTest(target=name):
                    self.assertIsInstance(reason, str)
                    self.assertGreater(
                        len(reason.strip()), 20,
                        "a reason must name the failure mode, not just assert "
                        "that one exists")

    def test_an_oracle_scope_names_a_real_oracle(self):
        """Step 9 scopes a delinked-only excuse with an `oracle` key.  A typo
        there silently widens the excuse back to every lane."""
        for name, row in sorted(self.targets.items()):
            scope = row.get("oracle")
            if scope is None:
                continue
            with self.subTest(target=name):
                self.assertIn(scope, ("xbe", "delinked"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
