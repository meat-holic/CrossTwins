#!/usr/bin/env python3
"""Emit the Hangul syllable block required by the Korean UI font."""

from pathlib import Path


repo_root = Path(__file__).resolve().parents[3]
output = repo_root / "lib/EpdFont/builtinFonts/ui-korean-character-set.txt"
output.write_text(
    "# Complete Hangul syllable block (11,172 codepoints)\nU+AC00-U+D7A3\n",
    encoding="utf-8",
    newline="\n",
)
print(f"Generated {output}: U+AC00-U+D7A3")
