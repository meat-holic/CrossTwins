"""Supported confirmation translations remain complete translated questions."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from gen_i18n import parse_yaml_file


class ConfirmationTranslationsTest(unittest.TestCase):
    def test_supported_locales_have_explicit_confirmation_headings(self):
        for language in ("english", "korean"):
            values = parse_yaml_file(str(ROOT / f"lib/I18n/translations/{language}.yaml"))
            with self.subTest(language=language):
                for key in ("STR_DELETE_ITEM_CONFIRM", "STR_SET_SLEEP_COVER_CONFIRM"):
                    self.assertTrue(values[key])
                    self.assertEqual(values[key], values[key].strip())
                    self.assertTrue(values[key].endswith("?"))

    def test_english_and_korean_resume_labels_are_preserved(self):
        english = parse_yaml_file(str(ROOT / "lib/I18n/translations/english.yaml"))
        korean = parse_yaml_file(str(ROOT / "lib/I18n/translations/korean.yaml"))
        self.assertEqual(english["STR_RESUME"], "\u226bRead")
        self.assertEqual(korean["STR_RESUME"], "\uc774\uc5b4\uc77d\uae30")

    def test_callers_use_complete_question_keys(self):
        for filename, key in (
            ("src/activities/home/FileBrowserActivity.cpp", "STR_DELETE_ITEM_CONFIRM"),
            ("src/activities/library/LibraryListActivity.cpp", "STR_DELETE_ITEM_CONFIRM"),
            ("src/activities/util/BmpViewerActivity.cpp", "STR_SET_SLEEP_COVER_CONFIRM"),
        ):
            with self.subTest(filename=filename):
                text = (ROOT / filename).read_text()
                normalized = " ".join(text.split())
                self.assertIn(
                    f"makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr({key}),",
                    normalized,
                )
                self.assertNotIn('tr(STR_DELETE) + std::string("? ")', text)
                self.assertNotIn('tr(STR_SET_SLEEP_COVER) + std::string("?")', text)


if __name__ == "__main__":
    unittest.main()
