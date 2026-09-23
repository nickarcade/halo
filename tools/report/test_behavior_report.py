"""Tests behavior-result ingestion and dashboard evidence plumbing."""

import importlib.util
import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path


HERE = Path(__file__).resolve().parent


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


report = _load("generate_decomp_report", HERE / "generate_decomp_report.py")
importer = _load("import_behavior_results", HERE / "import_behavior_results.py")
syntax = _load("test_dashboard_js_syntax", HERE / "test_dashboard_js_syntax.py")


class TestBehaviorReport(unittest.TestCase):
    def _root(self):
        temp = tempfile.TemporaryDirectory()
        root = Path(temp.name)
        (root / "artifacts/batch_verify").mkdir(parents=True)
        return temp, root

    def _write_result(self, path, target, status, passed, seeds, reason=None):
        path.write_text(json.dumps({
            "target": target,
            "status": status,
            "passed": passed,
            "failed": 0,
            "errors": 0,
            "seeds": seeds,
            "coverage_pct": 89.9,
            "confidence": "moderate",
            "reason": reason,
        }), encoding="utf-8")

    def test_loader_preserves_latest_counts_and_targeted_cases_without_summing_runs(self):
        temp, root = self._root()
        self.addCleanup(temp.cleanup)
        old = root / "artifacts/batch_verify/old.json"
        new = root / "artifacts/batch_verify/new.json"
        self._write_result(old, "target", "pass", 2, 2)
        self._write_result(new, "target", "inconclusive", 100, 100, "vacuous_output")
        os.utime(old, (1000, 1000))
        os.utime(new, (2000, 2000))
        verdict = report._load_equiv_verdicts(str(root))["target"]
        self.assertEqual(verdict["status"], "inconclusive")
        self.assertEqual(verdict["passed"], 100)
        self.assertEqual(verdict["seeds"], 100)
        self.assertEqual(verdict["targeted_case_count"], 0)

    def test_loader_ignores_impossible_count_totals(self):
        temp, root = self._root()
        self.addCleanup(temp.cleanup)
        invalid = root / "artifacts/batch_verify/invalid.json"
        self._write_result(invalid, "target", "pass", 2, 1)
        invalid_data = json.loads(invalid.read_text(encoding="utf-8"))
        invalid_data["failed"] = 1
        invalid.write_text(json.dumps(invalid_data), encoding="utf-8")
        self.assertEqual(report._load_equiv_verdicts(str(root)), {})

    def test_loader_rejects_invalid_target_and_status(self):
        temp, root = self._root()
        self.addCleanup(temp.cleanup)
        invalid_target = root / "artifacts/batch_verify/invalid-target.json"
        invalid_target.write_text(json.dumps({
            "target": ["not-hashable"], "status": "pass", "passed": 1, "seeds": 1,
        }), encoding="utf-8")
        invalid_status = root / "artifacts/batch_verify/invalid-status.json"
        invalid_status.write_text(json.dumps({
            "target": "target", "status": "mystery", "passed": 1, "seeds": 1,
        }), encoding="utf-8")
        self.assertEqual(report._load_equiv_verdicts(str(root)), {})

    def test_renamed_target_joins_by_address_only_with_current_provenance(self):
        temp, root = self._root()
        self.addCleanup(temp.cleanup)
        source = root / "src/halo/objects/widgets/widgets.c"
        source.parent.mkdir(parents=True)
        source.write_text("void renamed(void) {}\n", encoding="utf-8")
        bounds = root / "tools/verify/function_bounds.json"
        bounds.parent.mkdir(parents=True)
        bounds.write_text(json.dumps({
            "_meta": {"xbe_md5": "pristine-xbe"},
            "0x136580": {"end": "0x1365a0"},
        }), encoding="utf-8")
        result_path = root / "artifacts/batch_verify/old_name.json"
        self._write_result(result_path, "old_name", "pass", 50, 50)
        result = json.loads(result_path.read_text(encoding="utf-8"))
        result["address"] = "0x136580"
        result["_report_provenance"] = {
            "schema": 1, "address": "0x136580",
            "source_path": "src/halo/objects/widgets/widgets.c",
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "reference_end": "0x1365a0", "xbe_md5": "pristine-xbe",
        }
        result_path.write_text(json.dumps(result), encoding="utf-8")
        verdicts = report._load_equiv_verdicts(str(root))
        self.assertEqual(verdicts["0x136580"]["status"], "pass")
        self.assertEqual(report._equivalence_evidence_summary(verdicts)["evidence_count"], 1)
        source.write_text("void renamed(void) { changed(); }\n", encoding="utf-8")
        self.assertNotIn("0x136580", report._load_equiv_verdicts(str(root)))

    def test_imported_cases_are_distinct_and_inconclusive_main_stays_inconclusive(self):
        temp, root = self._root()
        self.addCleanup(temp.cleanup)
        main = root / "pilot-main.json"
        self._write_result(main, "target", "inconclusive", 100, 100, "vacuous_output")
        specs = []
        for index in range(4):
            case = root / "artifacts/batch_verify" / ("case-%d.json" % index)
            self._write_result(case, "target", "pass", 3, 3)
            specs.append("case-%d=%s" % (index, case))
        importer.import_bundle(main, specs, root / "artifacts/batch_verify/imported", root)
        for index in range(4):
            case = root / "artifacts/batch_verify" / ("case-%d.json" % index)
            os.utime(case, (4102444800, 4102444800))
        verdict = report._load_equiv_verdicts(str(root))["target"]
        self.assertEqual(verdict["status"], "inconclusive")
        self.assertEqual(verdict["passed"], 100)
        self.assertEqual(verdict["targeted_case_count"], 4)
        self.assertEqual(verdict["targeted_case_passed"], 4)
        self.assertEqual([case["label"] for case in verdict["targeted_cases"]],
                         ["case-0", "case-1", "case-2", "case-3"])

    def test_import_rejects_malformed_and_mismatched_cases(self):
        temp, root = self._root()
        self.addCleanup(temp.cleanup)
        main = root / "main.json"
        self._write_result(main, "target", "pass", 1, 1)
        malformed = root / "malformed.json"
        malformed.write_text("[]", encoding="utf-8")
        with self.assertRaises(ValueError):
            importer.import_bundle(main, ["bad=%s" % malformed], root / "out", root)
        impossible = root / "impossible.json"
        self._write_result(impossible, "target", "pass", 2, 1)
        with self.assertRaises(ValueError):
            importer.import_bundle(main, ["bad-counts=%s" % impossible], root / "out", root)
        mismatch = root / "mismatch.json"
        self._write_result(mismatch, "other", "pass", 1, 1)
        with self.assertRaises(ValueError):
            importer.import_bundle(main, ["wrong=%s" % mismatch], root / "out", root)

    def test_behavior_template_renders_counts_and_inconclusive_label(self):
        template = syntax.dashboard_templates(
            (HERE / "generate_decomp_report.py").read_text(encoding="utf-8"))[0]
        self.assertIn("equiv_passed", template)
        self.assertIn("f.equiv_passed + '/' + f.equiv_seeds + ' passed", template)
        self.assertIn("targeted cases agree", template)
        self.assertIn("⚠ Inconclusive", template)

    def test_z3_proof_requires_a_successful_verdict(self):
        template = syntax.dashboard_templates(
            (HERE / "generate_decomp_report.py").read_text(encoding="utf-8"))[0]
        self.assertIn("f.equiv_proven && f.equiv_status === 'pass'", template)
        self.assertNotIn("if (f.equiv_proven) return true;", template)


if __name__ == "__main__":
    unittest.main()
