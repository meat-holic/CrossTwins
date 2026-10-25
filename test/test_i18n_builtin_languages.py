"""Regression tests for the firmware's supported built-in UI locales."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from gen_i18n import (  # noqa: E402
    SUPPORTED_BUILTIN_LANGS,
    load_translations,
    parse_builtin_langs,
)


TRANSLATIONS_DIR = ROOT / "lib" / "I18n" / "translations"


class BuiltinLanguageGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (
            cls.languages,
            cls.language_names,
            cls.language_bcp47,
            cls.string_keys,
            cls.translations,
            cls.inherited_sets,
        ) = load_translations(str(TRANSLATIONS_DIR))

    def generate(self, output_dir):
        output_dir.mkdir(parents=True, exist_ok=True)
        return subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts" / "gen_i18n.py"),
                str(TRANSLATIONS_DIR),
                str(output_dir),
                "--src-dirs",
                str(ROOT / "src"),
                str(ROOT / "lib"),
                "--strip-unused",
                "--builtin-langs",
                "all",
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=True,
        )

    def test_default_and_all_select_only_english_and_korean(self):
        self.assertEqual(SUPPORTED_BUILTIN_LANGS, ("EN", "KO"))
        self.assertEqual(parse_builtin_langs(None, self.languages), {"EN", "KO"})
        self.assertEqual(parse_builtin_langs("all", self.languages), {"EN", "KO"})
        self.assertEqual(parse_builtin_langs("ko", self.languages), {"EN", "KO"})

    def test_only_supported_translation_yaml_files_are_required(self):
        yaml_names = sorted(path.name for path in TRANSLATIONS_DIR.glob("*.yaml"))
        self.assertEqual(yaml_names, ["english.yaml", "korean.yaml"])

    def test_legacy_enum_indices_and_keyboard_language_names_are_preserved(self):
        expected_codes = [
            "EN", "AR", "AZ", "BE", "BG", "BS", "CA", "CAV", "CS", "DA", "DE", "ES",
            "FA", "FI", "FR", "HE", "HU", "ID", "IT", "KK", "KO", "LT", "NB", "NL",
            "OOK", "PL", "PT", "P2", "RO", "RU", "SK", "SI", "SV", "TR", "UK", "VI",
        ]
        self.assertEqual(self.languages, expected_codes)

        expected_keyboard_names = {
            "EN": "English",
            "FR": "Français",
            "DE": "Deutsch",
            "ES": "Español",
            "RU": "Русский",
            "UK": "Українська",
            "BE": "Беларуская",
            "KK": "Қазақша",
            "HE": "עברית",
            "AR": "العربية",
        }
        for code, name in expected_keyboard_names.items():
            with self.subTest(code=code):
                self.assertEqual(self.language_names[self.languages.index(code)], name)

    def test_known_but_unsupported_and_unknown_codes_are_rejected(self):
        for code in ("FR", "DE", "AR", "ZZ"):
            with self.subTest(code=code), self.assertRaises(ValueError):
                parse_builtin_langs(code, self.languages)

    def test_generated_tables_are_limited_and_reproducible(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = root / "first"
            second = root / "second"
            first_result = self.generate(first)
            second_result = self.generate(second)
            self.assertIn("Languages: 2 of 36 built in (EN, KO)", first_result.stdout)
            self.assertEqual(first_result.stdout, second_result.stdout)

            filenames = ("I18nKeys.h", "I18nStrings.h", "I18nStrings.cpp")
            for filename in filenames:
                with self.subTest(file=filename):
                    self.assertEqual(
                        (first / filename).read_bytes(), (second / filename).read_bytes()
                    )

            keys = (first / "I18nKeys.h").read_text(encoding="utf-8")
            strings_h = (first / "I18nStrings.h").read_text(encoding="utf-8")
            strings_cpp = (first / "I18nStrings.cpp").read_text(encoding="utf-8")

            self.assertIn("extern const char STRINGS_EN_DATA[];", strings_h)
            self.assertIn("extern const char STRINGS_KO_DATA[];", strings_h)
            self.assertNotIn("STRINGS_FR_DATA", strings_h)
            self.assertNotIn("STRINGS_DE_DATA", strings_h)
            self.assertIn("const char STRINGS_EN_DATA[]", strings_cpp)
            self.assertIn("const char STRINGS_KO_DATA[]", strings_cpp)
            self.assertNotIn("STRINGS_FR_DATA", strings_cpp)
            self.assertNotIn("OFFSETS_FR", strings_cpp)
            language_names = strings_cpp.split("const char* const LANGUAGE_NAMES[] = {", 1)[1]
            language_names = language_names.split("};", 1)[0]
            self.assertEqual(language_names.count(","), len(self.languages))
            self.assertIn("case Language::EN:", keys)
            self.assertIn("case Language::KO:", keys)
            self.assertNotIn("case Language::FR:", keys)

            enum_entries = re.findall(
                r"^\s{2}([A-Z0-9]{2,3})\s+=\s+(\d+)", keys, re.MULTILINE
            )
            code_by_index = {int(index): code for code, index in enum_entries}
            self.assertEqual(
                [code_by_index[index] for index in range(len(self.languages))],
                self.languages,
            )
            sorted_match = re.search(
                r"SORTED_LANGUAGE_INDICES\[\] = \{([^}]+)\}", keys
            )
            self.assertIsNotNone(sorted_match)
            sorted_codes = {
                code_by_index[int(index.strip())]
                for index in sorted_match.group(1).split(",")
            }
            self.assertEqual(sorted_codes, {"EN", "KO"})

            for key in self.string_keys:
                with self.subTest(key=key):
                    self.assertEqual(len(self.translations[key]), len(self.languages))
                    for code in self.languages:
                        if code not in SUPPORTED_BUILTIN_LANGS:
                            self.assertEqual(
                                self.translations[key][self.languages.index(code)],
                                self.translations[key][0],
                            )


if __name__ == "__main__":
    unittest.main()
