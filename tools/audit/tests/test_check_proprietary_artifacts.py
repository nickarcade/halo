import importlib.util
import gzip
import io
import struct
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "check_proprietary_artifacts.py"
SPEC = importlib.util.spec_from_file_location("artifact_guard", SCRIPT)
guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(guard)


class ArtifactGuardTests(unittest.TestCase):
    def test_blocks_known_paths_and_extensions(self):
        cases = (
            "halo-patched/default.xbe",
            "delinked/functions/00123456.obj",
            "symbols/cachebeta.pdb",
            "halo_decompiled/cachebeta.elf.c",
            "halo_2276_functions.txt",
            "artifacts/gamestate_full.bin",
        )
        for path in cases:
            with self.subTest(path=path):
                self.assertIsNotNone(guard.path_reason(path))

    def test_allows_source_metadata_and_synthetic_fixture(self):
        cases = (
            "src/halo/game/game.c",
            "recovery/evidence/example.json",
            "css/bootstrap.min.css.map",
            "artifacts/snapshots/d1f40_synthetic.json",
        )
        for path in cases:
            with self.subTest(path=path):
                self.assertIsNone(guard.path_reason(path))

    def test_detects_binary_signatures(self):
        self.assertEqual(guard.content_reason("renamed.dat", b"XBEH" + b"\0" * 32),
                         "XBE file signature")
        self.assertEqual(
            guard.content_reason("renamed.dat", b"Microsoft C/C++ MSF 7.00"),
            "Microsoft PDB signature",
        )

    def test_detects_large_decompiler_export(self):
        data = b"\n".join(b"void FUN_%08x(void);" % i for i in range(12000))
        self.assertIn("decompiler", guard.content_reason("export.c", data))

    def test_detects_hex_encoded_memory(self):
        data = b'{"regions":{"0x80000000":"' + b"ab" * 40000 + b'"}}'
        self.assertIn("raw memory", guard.content_reason("capture.json", data))

    def test_detects_fragmented_hex_corpus(self):
        import json
        data = json.dumps({hex(0x80000000+i*4): "abcdef01"
                           for i in range(1200)}).encode()
        self.assertIn("raw memory", guard.content_reason("renamed.json", data))

    def test_reserved_fixture_directory_does_not_allow_objects(self):
        self.assertIsNotNone(guard.path_reason(
            "tools/equivalence/regression_snapshots/captured.obj"))
        self.assertIsNotNone(guard.path_reason("recording.halorec"))
        self.assertIsNotNone(guard.path_reason(".hidden/target.xbe"))

    def test_live_capture_is_rejected_even_when_small(self):
        data = b'{"description":"live snapshot","regions":{"0x10":"00000000"}}'
        self.assertIn("capture", guard.content_reason("fixture.json", data))

    def test_synthetic_exception_requires_exact_digest(self):
        import hashlib
        data = b'{"regions":{"0x80000000":"' + b"00" * 5000 + b'"}}'
        digest = hashlib.sha256(data).hexdigest()
        with patch.object(guard, "REVIEWED_SYNTHETIC", {"fixture.json": digest}):
            self.assertIsNone(guard.content_reason("fixture.json", data))
            self.assertIsNotNone(guard.content_reason("fixture.json", data + b" "))

    def test_renamed_coff_and_executable_are_rejected(self):
        coff = struct.pack("<HHIIIHH", 0x14c, 1, 0, 0, 0, 0, 0) + bytes(40)
        self.assertIn("COFF", guard.content_reason("innocent.data", coff))
        self.assertIn("executable", guard.content_reason("innocent.data", b"MZ" + bytes(100)))

    def test_containers_are_inspected(self):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w") as archive:
            archive.writestr("notes.dat", b"XBEH" + bytes(40))
        self.assertIn("XBE", guard.content_reason("source.zip", stream.getvalue()))
        self.assertIn("memory-recording", guard.content_reason(
            "trace.dat", gzip.compress(b"HMRC" + bytes(40))))
        self.assertIn("could not be inspected", guard.content_reason("broken.zip", b"PK\x03\x04"))

    def test_harmless_source_archive_is_allowed(self):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w") as archive:
            archive.writestr("analysis.py", "print('synthetic example')\n")
        self.assertIsNone(guard.content_reason("source.zip", stream.getvalue()))

    def test_commit_range_catches_artifact_deleted_before_tip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root)
            git("init", "-q")
            git("config", "user.email", "fixture@example.invalid")
            git("config", "user.name", "Synthetic fixture")
            (root / "README.md").write_text("Independent source\n")
            git("add", "README.md")
            git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "base")
            base = git("rev-parse", "HEAD").decode().strip()
            (root / "renamed.data").write_bytes(b"XBEH" + bytes(32))
            git("add", "renamed.data")
            git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "artifact")
            git("rm", "-q", "renamed.data")
            git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "delete")
            head = git("rev-parse", "HEAD").decode().strip()
            with patch.object(guard, "ROOT", root):
                self.assertEqual(guard.scan(guard.tracked_paths(), False), [])
                findings, count = guard.scan_range(base, head)
                self.assertGreater(count, 0)
                self.assertTrue(any("XBE" in reason for _, reason in findings))

    def test_staged_scan_reads_index_instead_of_working_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.check_call(["git", "init", "-q"], cwd=root)
            (root / "notes.dat").write_bytes(b"XBEH" + bytes(32))
            subprocess.check_call(["git", "add", "notes.dat"], cwd=root)
            (root / "notes.dat").write_text("Harmless working copy\n")
            with patch.object(guard, "ROOT", root):
                findings = guard.scan(guard.staged_paths(), True)
                self.assertEqual(len(findings), 1)
                self.assertIn("XBE", findings[0][1])

    def test_census_finds_deleted_artifact_and_every_alias(self):
        import json
        spec = importlib.util.spec_from_file_location(
            "history_census", SCRIPT.with_name("census_provenance_history.py"))
        census = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(census)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root)
            git("init", "-q")
            git("config", "user.email", "fixture@example.invalid")
            git("config", "user.name", "Synthetic fixture")
            (root / "first.dat").write_bytes(b"XBEH" + bytes(32))
            git("add", "first.dat")
            git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "fixture")
            oid = git("rev-parse", "HEAD:first.dat").decode().strip()
            tree = subprocess.check_output(["git", "mktree"], cwd=root,
                    input=("100644 blob " + oid + "\tprivate.dat\n").encode()).decode().strip()
            git("update-ref", "refs/private/tree", tree)
            git("mv", "first.dat", "alias.dat")
            git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "rename")
            git("rm", "-q", "alias.dat")
            git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "delete")
            output = root / "metadata"
            census.census(root, output)
            candidates = json.loads((output / "candidates.json").read_text())
            self.assertEqual(len(candidates), 1)
            self.assertEqual(candidates[0]["paths"], ["alias.dat", "first.dat", "private.dat"])
            self.assertIn("XBE signature", candidates[0]["review_reasons"])
            changes = json.loads((output / "changes.json").read_text())
            self.assertEqual({r[1] for r in changes["alias.dat"]}, {"A", "D"})
            snapshots = json.loads((output / "tree_snapshots.json").read_text())
            self.assertEqual(snapshots[0]["path"], "private.dat")


if __name__ == "__main__":
    unittest.main()
