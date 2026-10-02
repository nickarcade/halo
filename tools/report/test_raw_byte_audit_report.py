#!/usr/bin/env python3
"""Tests dashboard loading of fresh strict raw-byte audit evidence."""

import importlib.util
import json
import tempfile
import unittest
from types import SimpleNamespace
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("generate_decomp_report", HERE / "generate_decomp_report.py")
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)


class TestMeasurementManifest(unittest.TestCase):
    def test_header_and_toolchain_changes_invalidate_displayed_byte_records(self):
        from unittest.mock import patch
        import hashlib
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / "header.h"
            header.write_text("before")
            manifest = root / "snapshot.json"
            environment = {"compiler": "known"}
            document = {"metric": "raw_xbe_aligned_byte_lower_bound",
                        "inputs": {"header.h": hashlib.sha256(header.read_bytes()).hexdigest()},
                        "environment": environment}
            manifest.write_text(json.dumps(document))
            record = {"measurement_manifest": str(manifest)}
            with patch("tools.verify.byte_regression.environment_manifest", return_value=environment):
                self.assertTrue(report._measurement_manifest_valid(record, str(root), {}))
                header.write_text("after")
                self.assertFalse(report._measurement_manifest_valid(record, str(root), {}))
                header.write_text("before")
            with patch("tools.verify.byte_regression.environment_manifest", return_value={"compiler": "new"}):
                self.assertFalse(report._measurement_manifest_valid(record, str(root), {}))


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
            "schema_version": 2,
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
                "uncertain_relocations": 1,
                "mismatched_relocations": 0, "mismatched_relocation_bytes": 0,
                "accuracy_is_provisional": True,
            },
            "verification": {
                "boundaries": {"status": "inferred", "detail": "kind=auto"},
                "compiler": {"status": "provisional", "detail": "clang"},
                "behavior": {"status": "untested", "detail": "this static audit does not execute either function"},
                "instruction_operands": {"status": "exact", "detail": "encoding"},
                "literal_bytes": {"status": "unverified", "detail": "relocations"},
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
        self.assertEqual(audits["0x1000"]["verification"]["behavior"]["status"], "untested")
        self.assertEqual(audits["0x1000"]["verification"]["instruction_operands"]["status"], "exact")
        totals = report._raw_xbe_structural_totals(audits)
        self.assertEqual(totals["schema_version"], 2)
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
        self.assertEqual(totals["aligned_uncertain_bytes"], 1)
        self.assertEqual(totals["aligned_mismatched_relocations"], 0)
        self.assertEqual(totals["aligned_provisional_functions"], 1)

    def test_loads_fresh_structural_error_record_without_candidate_or_bounds(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        error = {
            "schema_version": 2,
            "lane": "raw_xbe_structural",
            "address": "0x00001000",
            "function": "target",
            "generated_at": "2026-09-22T00:00:00+00:00",
            "verdict": "not comparable",
            "confidence": "low",
            "reason": "VC71 compilation failed",
            "candidate": {"path": "candidate.obj"},
            "reference": {"sha256": report._file_hash(str(xbe), "sha256")},
            "verification": {
                "boundaries": {"status": "uncertain", "detail": "unknown"},
                "compiler": {"status": "provisional", "detail": "failed"},
                "behavior": {"status": "untested", "detail": "static"},
                "instruction_operands": {"status": "unverified", "detail": "none"},
                "literal_bytes": {"status": "unverified", "detail": "none"},
            },
        }
        (root / "artifacts/raw_xbe_structural/error.json").write_text(
            json.dumps(error), encoding="utf-8")
        audits = report._load_raw_xbe_structural_audits(str(root))
        self.assertEqual(audits["0x1000"]["verdict"], "not comparable")
        self.assertEqual(audits["0x1000"]["reason"], "VC71 compilation failed")

    def test_structural_totals_aggregate_aligned_range(self):
        audit = {
            "verdict": "structural differ",
            "reference": {"length": 4},
            "matching_non_relocation_bytes": 3,
            "byte_counts": {"non_relocation": 4},
            "aligned_byte_match": {
                "status": "scored", "matching_bytes": 2,
                "compared_bytes": 4, "uncertain_relocation_bytes": 1,
                "uncertain_relocations": 1,
                "mismatched_relocations": 2, "mismatched_relocation_bytes": 4,
                "accuracy_is_provisional": True,
            },
        }
        totals = report._raw_xbe_structural_totals({"0x1000": audit})
        self.assertEqual(totals["aligned_scored_functions"], 1)
        self.assertEqual(totals["aligned_byte_accuracy_lower"], 0.5)
        self.assertEqual(totals["aligned_byte_accuracy_upper"], 0.75)
        self.assertEqual(totals["aligned_provisional_functions"], 1)
        self.assertEqual(totals["aligned_mismatched_relocations"], 2)
        self.assertEqual(totals["aligned_mismatched_relocation_bytes"], 4)

    def test_dashboard_structural_population_scopes_to_ported_game_functions(self):
        units = [{
            "synthetic": False,
            "functions": [
                {
                    "ported": True,
                    "raw_xbe_structural_verdict": "structural differ",
                    "raw_xbe_structural_reference_length": 10,
                    "raw_xbe_structural_matching_non_relocation_bytes": 7,
                    "raw_xbe_structural_non_relocation_bytes": 9,
                    "raw_xbe_aligned_status": "scored",
                    "raw_xbe_aligned_matching_bytes": 7,
                    "raw_xbe_aligned_compared_bytes": 10,
                    "raw_xbe_aligned_uncertain_bytes": 1,
                },
                {
                    "ported": True,
                    "raw_xbe_structural_verdict": "not comparable",
                    "raw_xbe_aligned_status": "unavailable",
                },
                {"ported": True},
                {"ported": False, "raw_xbe_structural_verdict": "structural exact"},
            ],
        }, {
            "synthetic": True,
            "functions": [{"ported": True, "raw_xbe_aligned_status": "scored"}],
        }]
        summary = report._raw_xbe_structural_dashboard_totals(units)
        self.assertEqual(summary["implemented_functions"], 3)
        self.assertEqual(summary["compared_functions"], 1)
        self.assertEqual(summary["cannot_compare_functions"], 1)
        self.assertEqual(summary["unchecked_functions"], 1)
        self.assertEqual(summary["aligned_matching_bytes"], 7)
        self.assertEqual(summary["aligned_compared_bytes"], 10)
        self.assertEqual(summary["aligned_uncertain_bytes"], 1)
        self.assertEqual(summary["aligned_byte_accuracy_lower"], 0.7)

    def test_rejects_obsolete_schema_v1_structural_audit(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        record["schema_version"] = 1
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_compute_unit_stats_propagates_structural_evidence(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        record["aligned_byte_match"].update({
            "mismatched_relocations": 2,
            "mismatched_relocation_bytes": 4,
        })
        audit_path = root / "artifacts/raw_xbe_structural/target.json"
        audit_path.write_text(json.dumps(record), encoding="utf-8")
        audits = report._load_raw_xbe_structural_audits(str(root))

        symbol = report.Function("void target(void)", addr=0x1000, ported=True)
        kb = SimpleNamespace(
            addr_to_symbol={"4096": symbol},
            symbol_to_object={symbol: "target.obj"},
            object_to_source={"target.obj": str(source)},
        )
        store = SimpleNamespace(symbols={})
        function_cache = {"functions": {"0x1000": {"size": 4}}}
        units = report.compute_unit_stats(
            kb, store, function_cache,
            vc71_scores={"scores": {"target": {"score": 99.0}}},
            raw_xbe_structural_audits=audits,
        )[0]

        function = units[0]["functions"][0]
        self.assertEqual(function["match_percent"], 99.0)
        self.assertEqual(function["match_metric"], "mnemonic_similarity_not_raw_bytes")
        self.assertEqual(function["raw_xbe_aligned_lower"], record["aligned_byte_match"]["byte_accuracy"])
        self.assertNotEqual(function["match_percent"], function["raw_xbe_aligned_lower"] * 100)
        self.assertEqual(function["raw_xbe_verification"]["behavior"]["status"], "untested")
        self.assertEqual(function["raw_xbe_aligned_mismatched_relocations"], 2)
        self.assertEqual(function["raw_xbe_aligned_uncertain_relocations"], 1)
        self.assertEqual(units[0]["raw_xbe_structural"]["aligned_mismatched_relocations"], 2)
        self.assertEqual(units[0]["raw_xbe_structural"]["aligned_mismatched_relocation_bytes"], 4)
        mnemonic_only = report.compute_unit_stats(
            kb, store, function_cache,
            vc71_scores={"scores": {"target": {"score": 99.0}}},
        )[0][0]["functions"][0]
        self.assertEqual(mnemonic_only["match_percent"], 99.0)
        self.assertIsNone(mnemonic_only["raw_xbe_aligned_lower"])
        bytes_only = report.compute_unit_stats(
            kb, store, function_cache, raw_xbe_structural_audits=audits,
        )[0][0]["functions"][0]
        self.assertIsNone(bytes_only["match_percent"])
        self.assertEqual(bytes_only["raw_xbe_aligned_lower"], function["raw_xbe_aligned_lower"])

    def test_rejects_stale_structural_source_hash(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        source.write_text("void target(void) { return; }\n", encoding="utf-8")
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_rejects_moved_structural_bound(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        bounds.write_text(json.dumps({"0x1000": {"end": "0x1008", "kind": "auto"}}), encoding="utf-8")
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(report._load_raw_xbe_structural_audits(str(root)), {})

    def test_keeps_structural_record_when_unrelated_bounds_change(self):
        temp, root, source, xbe = self._make_root()
        self.addCleanup(temp.cleanup)
        bounds = root / "tools/verify/function_bounds.json"
        record = self._structural_record(source, xbe, bounds)
        bounds.write_text(json.dumps({
            "0x1000": {"end": "0x1004", "kind": "auto", "name": "renamed"},
            "0x2000": {"end": "0x2010", "kind": "auto"}}), encoding="utf-8")
        (root / "artifacts/raw_xbe_structural/target.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertIn("0x1000", report._load_raw_xbe_structural_audits(str(root)))

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


class TestDashboardByteRefresh(unittest.TestCase):
    def test_refresh_response_does_not_supply_mnemonic_scores(self):
        from unittest.mock import Mock
        spec = importlib.util.spec_from_file_location("progress_server", HERE / "progress_server.py")
        server = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(server)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            original = {"units": [{"name": "target", "source_path": "source.c",
                                   "functions": [{"match_percent": 99.0,
                                                  "raw_xbe_aligned_lower": 0.5}]}]}
            path.write_text(json.dumps(original))
            handler = object.__new__(server.SSEHandler)
            handler.directory = directory
            handler._run_raw_audit = Mock(return_value={"returncode": 0, "totals": {"byte_accuracy": 0.75}})
            handler._refresh_dashboard = Mock(return_value=True)
            result = handler._run_score("target")
            self.assertEqual(result["metric"], "raw_xbe_aligned_byte_lower_bound")
            self.assertNotIn("scores", result)
            handler._run_raw_audit.assert_called_once_with("target", "source.c")
            handler._refresh_dashboard.assert_called_once()
            self.assertEqual(json.loads(path.read_text()), original)
            handler._run_raw_audit.return_value = {"returncode": 2}
            handler._refresh_dashboard.reset_mock()
            self.assertIsNone(handler._run_score("target"))
            handler._refresh_dashboard.assert_not_called()


if __name__ == "__main__":
    unittest.main()
