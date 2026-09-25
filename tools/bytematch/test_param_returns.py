#!/usr/bin/env python3
"""Tests for bytematch's report-only parameter-return transformation."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bytematch as b  # noqa: E402


def transform(source, function="f", param=1):
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "t.c"
        with open(path, "w", newline="") as handle:
            handle.write(source)
        return b.param_return_variant(path, function, param, source)


class ParamReturnVariantTest(unittest.TestCase):
    def test_fallthrough_returns_named_pointer_param(self):
        source = "void f(int a, float *result)\n{\n  *result = a;\n}\n"
        out, return_type, name = transform(source)
        self.assertEqual(return_type, "float *")
        self.assertEqual(name, "result")
        self.assertIn("float * f(int a, float *result)", out)
        self.assertIn("*result = a;\nreturn result; }", out)
        self.assertEqual(out.count("\n"), source.count("\n"))

    def test_bare_returns_get_the_param(self):
        source = "void f(int a, int *out)\n{\n  if (a) return;\n  *out = a;\n}\n"
        out, _, _ = transform(source)
        self.assertIn("if (a) return out;", out)
        self.assertIn("return out; }", out)

    def test_existing_value_return_is_refused(self):
        source = "void f(int a, int *out)\n{\n  if (a) return out;\n}\n"
        out, reason, _ = transform(source)
        self.assertIsNone(out)
        self.assertIn("already returns a value", reason)

    def test_non_void_definition_is_refused(self):
        out, reason, _ = transform("int *f(int a, int *out)\n{\n  return out;\n}\n")
        self.assertIsNone(out)
        self.assertIn("does not start with void", reason)

    def test_bad_param_index_is_refused(self):
        out, reason, _ = transform("void f(int a)\n{\n}\n", param=3)
        self.assertIsNone(out)
        self.assertIn("parameter 3", reason)

    def test_named_scalar_param_becomes_scalar_return(self):
        source = "void f(int a, unsigned short value)\n{\n  g = value;\n}\n"
        out, return_type, name = transform(source)
        self.assertEqual((return_type, name), ("unsigned short", "value"))
        self.assertIn("unsigned short f(", out)
        self.assertIn("return value; }", out)

    def test_crlf_is_preserved(self):
        source = "void f(int a, int *out)\r\n{\r\n  *out = a;\r\n}\r\n"
        out, _, _ = transform(source)
        self.assertEqual(out.count("\r\n"), source.count("\r\n"))
        self.assertNotIn("\n", out.replace("\r\n", ""))


if __name__ == "__main__":
    unittest.main()
