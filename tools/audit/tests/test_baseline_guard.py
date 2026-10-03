import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


# A git hook inherits GIT_INDEX_FILE (and friends) and the outer commit's
# HALO_STAGED_DIR file list; the fixture repos below must use neither.
for _var in ("GIT_INDEX_FILE", "GIT_DIR", "GIT_WORK_TREE", "GIT_PREFIX",
             "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES",
             "HALO_STAGED_DIR"):
    os.environ.pop(_var, None)

TOOLS = Path(__file__).resolve().parents[2]
HOOK = "tools/hooks/pre-commit-baseline-guard.sh"
BASELINE = "tools/kb_reg_baseline.json"

ENTRY = "void f(int a@<eax>, char b@<bl>);"


class BaselineGuardTests(unittest.TestCase):
    def setUp(self):
        self.repo = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.repo)
        for rel in (HOOK, "tools/hooks/lib-staged.sh", "tools/audit/extract_reg_args.py"):
            (self.repo / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(TOOLS.parent / rel, self.repo / rel)
        self._git("init", "-q")
        self._git("config", "user.email", "test@example.invalid")
        self._git("config", "user.name", "test")
        self._write({"0x1000": ENTRY})
        self._git("add", "-A")
        self._git("commit", "-q", "-m", "base")

    def _git(self, *args):
        subprocess.run(["git", *args], cwd=self.repo, check=True,
                       capture_output=True)

    def _write(self, functions):
        (self.repo / BASELINE).write_text(json.dumps({"functions": functions}))

    def _guard(self, functions):
        self._write(functions)
        self._git("add", BASELINE)
        return subprocess.run(["bash", HOOK], cwd=self.repo,
                              capture_output=True, text=True)

    def test_allows_addition(self):
        result = self._guard({"0x1000": ENTRY, "0x2000": "void g(int x@<ecx>);"})
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_allows_type_and_name_change(self):
        result = self._guard(
            {"0x1000": "int f(int16_t index@<eax>, char force@<bl>);"})
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_blocks_removal(self):
        result = self._guard({})
        self.assertEqual(result.returncode, 1)
        self.assertIn("REMOVED", result.stdout)

    def test_blocks_register_change(self):
        result = self._guard({"0x1000": "void f(int a@<ecx>, char b@<bl>);"})
        self.assertEqual(result.returncode, 1)
        self.assertIn("CHANGED register shape", result.stdout)

    def test_blocks_dropped_annotation(self):
        result = self._guard({"0x1000": "void f(int a, char b@<bl>);"})
        self.assertEqual(result.returncode, 1)

    def test_blocks_parameter_count_change(self):
        result = self._guard({"0x1000": "void f(int a@<eax>, char b@<bl>, int c);"})
        self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
