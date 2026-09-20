#!/usr/bin/env python3
"""Unit tests for kb_diff_sources.py's diffing logic (flatten/to_repo_path).

Uses in-memory kb.json-shaped dicts rather than real git revisions, so this
stays correct across history rewrites (e.g. a branch rebase) that would
invalidate hard-coded commit SHAs.
"""
import unittest

from kb_diff_sources import flatten, to_repo_path


class TestToRepoPath(unittest.TestCase):
    def test_bare_relative_path_gets_src_halo_prefix(self):
        self.assertEqual(to_repo_path("ai/path.c"), "src/halo/ai/path.c")

    def test_already_repo_relative_path_is_unchanged(self):
        self.assertEqual(to_repo_path("src/halo/ai/path.c"), "src/halo/ai/path.c")

    def test_none_source_yields_none(self):
        self.assertIsNone(to_repo_path(None))

    def test_backslashes_are_normalized(self):
        self.assertEqual(to_repo_path("ai\\path.c"), "src/halo/ai/path.c")


class TestFlatten(unittest.TestCase):
    def test_grouped_function_uses_object_source(self):
        data = {"objects": [{"source": "ai/path.c", "functions": [
            {"addr": "0x100", "decl": "void f(void);", "ported": True},
        ]}]}
        out = flatten(data)
        self.assertEqual(out["0x100"][1], "ai/path.c")

    def test_per_function_source_path_overrides_object_source(self):
        data = {"objects": [{"source": "ai/path.c", "functions": [
            {"addr": "0x100", "source_path": "ai/other.c", "ported": True},
        ]}]}
        out = flatten(data)
        self.assertEqual(out["0x100"][1], "ai/other.c")

    def test_loose_top_level_entry_uses_file_field(self):
        data = {"0xdb250": {"addr": "0xdb250", "file": "src/halo/interface/event_manager.c"}}
        out = flatten(data)
        self.assertEqual(out["0xdb250"][1], "src/halo/interface/event_manager.c")

    def test_md5_and_objects_keys_are_not_treated_as_functions(self):
        data = {"md5": "abc", "objects": []}
        self.assertEqual(flatten(data), {})


class TestDiffSemantics(unittest.TestCase):
    """Mirrors the set-comparison loop in main(): same shape, no subprocess."""

    def _changed_sources(self, base, head):
        base_f, head_f = flatten(base), flatten(head)
        changed = set()
        for addr in set(base_f) | set(head_f):
            b, h = base_f.get(addr), head_f.get(addr)
            if b == h:
                continue
            for entry in (b, h):
                if entry is not None:
                    path = to_repo_path(entry[1])
                    if path:
                        changed.add(path)
        return changed

    def test_ported_flag_flip_is_detected(self):
        base = {"objects": [{"source": "game/game.c", "functions": [
            {"addr": "0xb5210", "ported": True},
        ]}]}
        head = {"objects": [{"source": "game/game.c", "functions": [
            {"addr": "0xb5210", "ported": False},
        ]}]}
        self.assertEqual(self._changed_sources(base, head), {"src/halo/game/game.c"})

    def test_unchanged_function_reports_nothing(self):
        kb = {"objects": [{"source": "game/game.c", "functions": [
            {"addr": "0xb5210", "ported": True},
        ]}]}
        self.assertEqual(self._changed_sources(kb, kb), set())

    def test_function_moved_between_files_reports_both(self):
        base = {"objects": [{"source": "old.c", "functions": [
            {"addr": "0x1", "ported": True},
        ]}]}
        head = {"objects": [{"source": "new.c", "functions": [
            {"addr": "0x1", "ported": True},
        ]}]}
        self.assertEqual(self._changed_sources(base, head),
                          {"src/halo/old.c", "src/halo/new.c"})

    def test_new_function_reports_its_file_only(self):
        base = {"objects": []}
        head = {"objects": [{"source": "new.c", "functions": [
            {"addr": "0x1", "ported": False},
        ]}]}
        self.assertEqual(self._changed_sources(base, head), {"src/halo/new.c"})

    def test_112_function_rename_collapses_to_owning_tus(self):
        # Regression pin for the real case this script exists for: a batch
        # rename touching many functions across few TUs must report those few
        # TUs, not one entry per function and never a full-repo signal.
        base = {"objects": [{"source": "hs/hs_runtime.c", "functions": [
            {"addr": hex(0x1000 + i), "decl": f"void FUN_{i}(void);", "ported": True}
            for i in range(50)
        ]}]}
        head = {"objects": [{"source": "hs/hs_runtime.c", "functions": [
            {"addr": hex(0x1000 + i), "decl": f"void renamed_{i}(void);", "ported": True}
            for i in range(50)
        ]}]}
        self.assertEqual(self._changed_sources(base, head), {"src/halo/hs/hs_runtime.c"})


if __name__ == "__main__":
    unittest.main()
