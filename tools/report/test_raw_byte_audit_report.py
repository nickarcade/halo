#!/usr/bin/env python3
"""Tests dashboard loading of fresh strict raw-byte audit evidence."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("generate_decomp_report", HERE / "generate_decomp_report.py")
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)


class TestRawByteAuditLoading(unittest.TestCase):
    def _make_root(self):
        temp = tempfile.TemporaryDirectory()
        root = Path(temp.name)
        (root / "artifacts/raw_byte_audit").mkdir(parents=True)
        (root / "artifacts/raw_xbe_structural").mkdir(parents=True)
        (root / "tools/verify").mkdir(parents=True)
        (root / "halo-patched").mkdir()
        source = root / "source.c"
        source.write_text("void target(void) {}\n", encoding="utf-8")
        xbe = root / "halo-patched/cachebeta.xbe"
        xbe.write_bytes(b"pristine")
        bounds = {"0x1000": {"end": "0x1004", "kind": "auto"}}
        (root / "tools/verify/function_bounds.json").write_text(json.dumps(bounds), encoding="utf-8")
        return temp, root, source, xbe

    def _record(self, source, xbe, verdict="raw-byte exact"):
        return {
            "address": "0x00001000",
            "function": "target",
            "generated_at": "2026-09-21T00:00:00+00:00",
            "verdict": verdict,
            "source": {"path": str(source), "sha256": report._file_hash(str(source), "sha256")},
            "candidate": {"sha256": "candidate-object", "length": 4},
            "reference": {
                "sha256": report._file_hash(str(xbe), "sha256"),
                "sha256_span": "reference-span",
                "start": "0x1000", "end": "0x1004", "length": 4,
            },
        }

    def test_loads_current_provenance_valid_audit(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        record = self._record(source, xbe)
        (root / "artifacts/raw_byte_audit/target.json").write_text(json.dumps(record), encoding="utf-8")
        audits = report._load_raw_byte_audits(str(root))
        self.assertEqual(audits["0x1000"]["verdict"], "raw-byte exact")
        self.assertEqual(audits["0x1000"]["artifact"], "artifacts/raw_byte_audit/target.json")

    def test_rejects_stale_source_hash(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        record = self._record(source, xbe)
        source.write_text("void target(void) { return; }\n", encoding="utf-8")
        (root / "artifacts/raw_byte_audit/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_byte_audits(str(root)), {})

    def _structural_record(self, source, xbe, bounds, verdict="structural exact"):
        return {
            "schema_version": 1,
            "lane": "raw_xbe_structural",
            "address": "0x00001000",
            "function": "target",
            "generated_at": "2026-09-21T00:00:00+00:00",
            "verdict": verdict,
            "confidence": "high",
            "byte_accuracy": 0.75,
            "matching_non_relocation_bytes": 3,
            "byte_counts": {"non_relocation": 4},
            "aligned_byte_match": {
                "status": "scored", "byte_accuracy": 0.5,
                "byte_accuracy_upper_bound": 0.75, "matching_bytes": 2,
                "compared_bytes": 4, "uncertain_relocation_bytes": 1,
                "accuracy_is_provisional": True,
            },
            "source": {"path": str(source), "sha256": report._file_hash(str(source), "sha256")},
            "candidate": {"sha256": "candidate-object", "length": 4},
            "bounds": {"start": "0x1000", "end": "0x1004", "sha256": report._file_hash(str(bounds), "sha256")},
            "reference": {
                "sha256": report._file_hash(str(xbe), "sha256"),
                "start": "0x1000", "end": "0x1004", "length": 4,
            },
        }

    def test_loads_current_provenance_valid_structural_audit(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        path = root / "artifacts/raw_xbe_structural/target.json"
        path.write_text(json.dumps(record), encoding="utf-8")
        audits = report._load_raw_xbe_structural_audits(str(root))
        self.assertEqual(audits["0x1000"]["verdict"], "structural exact")
        self.assertEqual(audits["0x1000"]["artifact"], "artifacts/raw_xbe_structural/target.json")
        totals = report._raw_xbe_structural_totals(audits)
        self.assertEqual(totals["audited_functions"], 1)
        self.assertEqual(totals["function_exact_rate"], 1.0)
        self.assertEqual(totals["matching_non_relocation_bytes"], 3)
        self.assertEqual(totals["non_relocation_bytes"], 4)
        self.assertEqual(totals["byte_accuracy"], 0.75)
        self.assertEqual(totals["aligned_scored_functions"], 1)
        self.assertEqual(totals["aligned_matching_bytes"], 2)
        self.assertEqual(totals["aligned_compared_bytes"], 4)
        self.assertEqual(totals["aligned_byte_accuracy_lower"], 0.5)
        self.assertEqual(totals["aligned_byte_accuracy_upper"], 0.75)
        self.assertEqual(totals["aligned_provisional_functions"], 1)

    def test_structural_totals_aggregate_aligned_range(self):
        audit = {
            "verdict": "structural differ",
            "reference": {"length": 4},
            "matching_non_relocation_bytes": 3,
            "byte_counts": {"non_relocation": 4},
            "aligned_byte_match": {
                "status": "scored", "matching_bytes": 2,
                "compared_bytes": 4, "uncertain_relocation_bytes": 1,
                "accuracy_is_provisional": True,
            },
        }
        totals = report._raw_xbe_structural_totals({"0x1000": audit})
        self.assertEqual(totals["aligned_scored_functions"], 1)
        self.assertEqual(totals["aligned_byte_accuracy_lower"], 0.5)
        self.assertEqual(totals["aligned_byte_accuracy_upper"], 0.75)
        self.assertEqual(totals["aligned_provisional_functions"], 1)

    def test_rejects_stale_structural_source_hash(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        source.write_text("void target(void) { return; }\n", encoding="utf-8")
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_rejects_stale_structural_bounds_hash(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        bounds.write_text(json.dumps({"0x1000": {"end": "0x1008", "kind": "auto"}}), encoding="utf-8")
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_rejects_structural_record_with_invalid_byte_counts(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        record = self._structural_record(source, xbe, root / "tools/verify/function_bounds.json")
        record["matching_non_relocation_bytes"] = 5
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_rejects_structural_record_without_stable_schema(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        record = self._structural_record(source, xbe, root / "tools/verify/function_bounds.json")
        del record["lane"]
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_rejects_moved_bound(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        record = self._record(source, xbe)
        (root / "tools/verify/function_bounds.json").write_text(
            json.dumps({"0x1000": {"end": "0x1008", "kind": "auto"}}), encoding="utf-8")
        (root / "artifacts/raw_byte_audit/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_byte_audits(str(root)), {})


if __name__ == "__main__":
    unittest.main()
