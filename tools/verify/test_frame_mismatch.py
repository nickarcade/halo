"""Tests for the prologue frame-size survey."""

import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import frame_mismatch as fm


class FrameSizeTest(unittest.TestCase):
    def test_reads_the_prologue_frame(self):
        # push ebp; mov ebp,esp; sub esp,0xd0
        code = b"\x55\x8b\xec\x81\xec\xd0\x00\x00\x00\xc3"
        self.assertEqual(fm.frame_size(code, 0x1000), 0xd0)

    def test_reads_a_short_form_frame(self):
        # push ebp; mov ebp,esp; sub esp,0x10
        code = b"\x55\x8b\xec\x83\xec\x10\xc3"
        self.assertEqual(fm.frame_size(code, 0x1000), 0x10)

    def test_frameless_function_reports_none(self):
        code = b"\x55\x8b\xec\xc3"
        self.assertIsNone(fm.frame_size(code, 0x1000))

    def test_ignores_a_later_dynamic_allocation(self):
        # A `sub esp` past the prologue window is alloca-like, not the frame.
        code = b"\x55\x8b\xec" + b"\x90" * 10 + b"\x83\xec\x10\xc3"
        self.assertIsNone(fm.frame_size(code, 0x1000))

    def test_sub_on_another_register_is_not_a_frame(self):
        # sub eax, 0x10
        code = b"\x55\x8b\xec\x83\xe8\x10\xc3"
        self.assertIsNone(fm.frame_size(code, 0x1000))


if __name__ == "__main__":
    unittest.main()
