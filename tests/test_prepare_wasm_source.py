#!/usr/bin/env python3
"""Regression tests for the bounded WASM source preparer."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PREPARER_PATH = ROOT / "tools" / "prepare_wasm_source.py"
SOURCE_PATH = ROOT / "src" / "hlolli_wg_double_bass.c"


def load_preparer():
    spec = importlib.util.spec_from_file_location(
        "hlolli_wg_double_bass_prepare_wasm_source", PREPARER_PATH
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {PREPARER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def literal_tokens(source: str) -> list[str]:
    """Return each C string or character token, including its quotes."""
    tokens: list[str] = []
    index = 0
    while index < len(source):
        quote = source[index]
        if quote not in ('"', "'"):
            index += 1
            continue
        start = index
        index += 1
        while index < len(source):
            character = source[index]
            index += 1
            if character == "\\" and index < len(source):
                index += 1
            elif character == quote:
                tokens.append(source[start:index])
                break
        else:
            raise AssertionError("unterminated literal in test input")
    return tokens


class PrepareWasmSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.preparer = load_preparer()

    def test_compaction_preserves_literal_bytes_and_escapes(self) -> None:
        source = r'''
#define DIAGNOSTIC "preprocessor: keep x == y and // text"
static const char *diagnostic =
    "voice -> body, x == y; keep // text and /* text */";
static const char *escaped =
    "quote: \"; slash: \\; tab escape: \t; question ? colon :";
static const char *continued = "first line \
second line";
static const int slash = '/';
static const int quote = '\'';
static const int backslash = '\\';
static const int space = ' ';
int total = left + right; /* remove this comment */
'''
        prepared = self.preparer.prepare(source)

        self.assertEqual(literal_tokens(prepared), literal_tokens(source))
        self.assertNotIn("remove this comment", prepared)
        self.assertIn("int total=left+right;", prepared)

    def test_preserves_legal_notices_only_in_code(self) -> None:
        notice = "/*! Copyright example. Permission to copy with this notice. */"
        source = notice + '\nconst char *text = "/*! literal, not a notice */";'
        prepared = self.preparer.prepare(source)
        self.assertTrue(prepared.startswith(notice + "\n"))
        self.assertEqual(prepared.count(notice), 1)
        self.assertIn('"/*! literal, not a notice */"', prepared)

    def test_canonical_source_remains_within_fixed_cap(self) -> None:
        source = SOURCE_PATH.read_text(encoding="utf-8")
        stripped = self.preparer.strip_comments(
            self.preparer.strip_test_sections(source)
        )
        prepared = self.preparer.prepare(source)
        byte_count = len(prepared.encode("utf-8"))

        self.assertEqual(literal_tokens(prepared), literal_tokens(stripped))
        self.assertIn(
            r'"hlolli_wg_double_bass_create: cannot allocate manager\n"',
            prepared,
        )
        self.assertIn("Copyright (C) 1993 by Sun Microsystems", prepared)
        self.assertIn("Copyright (C) 2004 by Sun Microsystems", prepared)
        self.assertLessEqual(byte_count, self.preparer.SOURCE_LIMIT)


if __name__ == "__main__":
    unittest.main()
