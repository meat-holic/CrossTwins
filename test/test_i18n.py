"""KO/EN generator and runtime contracts, independent of firmware/SDK configuration.

Run with unittest. Runtime tests require I18N_TEST_CXX (or CXX) and
I18N_TEST_ARDUINOJSON_INCLUDE, unless the compiler and ArduinoJson are discoverable.
"""

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("gen_i18n", ROOT / "scripts/gen_i18n.py")
GEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GEN)
TRANSLATIONS = ROOT / "lib/I18n/translations"


def load_locales():
    return GEN.load_translations(str(TRANSLATIONS))


class LocaleGeneratorTest(unittest.TestCase):
    def test_only_english_and_korean_resources(self):
        self.assertEqual(
            {p.name for p in TRANSLATIONS.glob("*.yaml")},
            {"english.yaml", "korean.yaml"},
        )
        codes, names, tags, *_ = load_locales()
        self.assertEqual(codes, ["EN", "KO"])
        self.assertEqual(names, ["English", "한국어"])
        self.assertEqual(tags, ["en", "ko"])

    def test_ct_controls_and_korean_strings_survive(self):
        en = GEN.parse_yaml_file(str(TRANSLATIONS / "english.yaml"))
        ko = GEN.parse_yaml_file(str(TRANSLATIONS / "korean.yaml"))
        self.assertEqual(en["STR_CROSSTWINS"], "CrossTwins")
        self.assertEqual(ko["STR_SETTINGS_TITLE"], "설정")
        self.assertEqual(ko["STR_RESUME"], "이어읽기")
        for width in range(1, 6):
            key = "STR_INDENT_1_SPACE" if width == 1 else f"STR_INDENT_{width}_SPACES"
            self.assertEqual(en[key], str(width))
            self.assertIn(key, ko)
        for key in ("STR_BT_STARTING", "STR_SLIDESHOW_PLAY", "STR_KB_LAYOUT_AR"):
            self.assertIn(key, en)
            self.assertIn(key, ko)

    def test_all_current_source_keys_have_english_fallback(self):
        en = GEN.parse_yaml_file(str(TRANSLATIONS / "english.yaml"))
        used = GEN.find_used_string_keys([str(ROOT / "src"), str(ROOT / "lib")])
        self.assertTrue(used)
        self.assertEqual(used - en.keys(), set())

    def test_builtin_filter_keeps_english_and_rejects_unsupported_locale(self):
        codes, *_ = load_locales()
        self.assertEqual(GEN.parse_builtin_langs("ko", codes), {"EN", "KO"})
        with self.assertRaisesRegex(ValueError, "Unknown language code"):
            GEN.parse_builtin_langs("fr", codes)

    def test_generation_is_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="ct-i18n-generation-") as directory:
            out = Path(directory)
            with contextlib.redirect_stdout(io.StringIO()):
                GEN.main(str(TRANSLATIONS), str(out), [str(ROOT / "src"), str(ROOT / "lib")], True)
            first = {p.name: p.read_bytes() for p in out.iterdir()}
            with contextlib.redirect_stdout(io.StringIO()):
                GEN.main(str(TRANSLATIONS), str(out), [str(ROOT / "src"), str(ROOT / "lib")], True)
            self.assertEqual(first, {p.name: p.read_bytes() for p in out.iterdir()})
            self.assertEqual(set(first), {"I18nKeys.h", "I18nStrings.h", "I18nStrings.cpp"})


class LocaleRuntimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("I18N_TEST_CXX") or os.environ.get("CXX") or shutil.which("c++")
        json_include = Path(os.environ.get(
            "I18N_TEST_ARDUINOJSON_INCLUDE", ROOT / ".pio/libdeps/default/ArduinoJson/src"
        ))
        if not compiler or not (json_include / "ArduinoJson.h").is_file():
            raise RuntimeError("Set I18N_TEST_CXX and I18N_TEST_ARDUINOJSON_INCLUDE for runtime coverage")
        cls.temporary = tempfile.TemporaryDirectory(prefix="ct-i18n-runtime-")
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name)
        with contextlib.redirect_stdout(io.StringIO()):
            GEN.main(str(TRANSLATIONS), str(directory), [str(ROOT / "src"), str(ROOT / "lib")], True)
        # Keep generated headers adjacent to an exact copy of the production
        # source so stale ignored headers in the worktree cannot mask a failure.
        for name in ("I18n.h", "I18n.cpp"):
            shutil.copyfile(ROOT / "lib/I18n" / name, directory / name)
        settings = (ROOT / "src/CrossPointSettings.cpp").read_text(encoding="utf-8")
        load = re.search(r'  // Language -- stored as code string[^\n]*\n(.*?)\n\s*// Absent means', settings, re.S)
        save = re.search(r'  doc\["language"\] = [^;]+;', settings)
        if not load or not save or "<<<<<<<" in load.group(1):
            raise RuntimeError("Could not extract the production locale persistence statements")
        en = GEN.parse_yaml_file(str(TRANSLATIONS / "english.yaml"))
        ko = GEN.parse_yaml_file(str(TRANSLATIONS / "korean.yaml"))
        used = GEN.find_used_string_keys([str(ROOT / "src"), str(ROOT / "lib")])
        fallback = next(k for k in en if k.startswith("STR_") and k not in ko and k in used)
        harness = r'''
#include "I18n.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstring>
#include <iterator>

uint8_t loadLanguage(const char* json, uint8_t language) {
  JsonDocument doc;
  assert(!deserializeJson(doc, json));
@LOAD@
  return language;
}

void checkSavedLanguage(uint8_t language, const char* expected) {
  JsonDocument doc;
@SAVE@
  assert(strcmp(doc["language"].as<const char*>(), expected) == 0);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  if (strcmp(argv[1], "registry") == 0) {
    static_assert(getLanguageCount() == 2);
    static_assert(static_cast<uint8_t>(Language::EN) == 0);
    static_assert(static_cast<uint8_t>(Language::KO) == 1);
    static_assert(std::size(SORTED_LANGUAGE_INDICES) == 2);
    assert(SORTED_LANGUAGE_INDICES[0] == 0 && SORTED_LANGUAGE_INDICES[1] == 1);
    assert(I18N.getLanguage() == Language::EN);
    assert(strcmp(I18N.getLanguageName(Language::KO), "한국어") == 0);
    assert(strstr(I18n::getCharacterSet(Language::KO), "한") != nullptr);
  } else if (strcmp(argv[1], "translations") == 0) {
    I18N.setLanguage(Language::EN);
    assert(strcmp(tr(STR_SETTINGS_TITLE), "Settings") == 0);
    I18N.setLanguage(Language::KO);
    assert(strcmp(tr(STR_SETTINGS_TITLE), "설정") == 0);
    assert(strcmp(tr(@FALLBACK_KEY@), @FALLBACK_VALUE@) == 0);
    assert(strcmp(I18N.get(static_cast<StrId>(65535)), "???") == 0);
  } else if (strcmp(argv[1], "codes") == 0) {
    assert(I18n::languageFromCode("KO") == Language::KO);
    assert(I18n::languageFromCode("EN") == Language::EN);
    for (const char* code : {"FR", "AZ", "", "ko", "UNKNOWN"}) {
      assert(I18n::languageFromCode(code) == Language::EN);
    }
    assert(I18n::languageFromCode(nullptr) == Language::EN);
    I18N.setLanguage(Language::KO);
    I18N.setLanguage(static_cast<Language>(255));
    assert(I18N.getLanguage() == Language::KO);
  } else if (strcmp(argv[1], "load") == 0) {
    const uint8_t korean = static_cast<uint8_t>(Language::KO);
    assert(loadLanguage(R"({"language":"KO"})", 0) == korean);
    assert(loadLanguage(R"({"language":"EN"})", korean) == 0);
    for (const char* json : {R"({"language":"FR"})", R"({"language":"AZ"})",
                            R"({"language":"ko"})", R"({"language":""})", "{}",
                            R"({"language":null})", R"({"language":1})",
                            R"({"language":true})", R"({"language":[]})",
                            R"({"language":{}})"}) {
      assert(loadLanguage(json, korean) == 0);
    }
  } else if (strcmp(argv[1], "save") == 0) {
    checkSavedLanguage(static_cast<uint8_t>(Language::KO), "KO");
    checkSavedLanguage(static_cast<uint8_t>(Language::EN), "EN");
    checkSavedLanguage(255, "EN");
  } else {
    return 2;
  }
}
'''
        harness = (harness.replace("@LOAD@", load.group(1)).replace("@SAVE@", save.group(0))
                   .replace("@FALLBACK_KEY@", fallback)
                   .replace("@FALLBACK_VALUE@", json.dumps(en[fallback], ensure_ascii=False)))
        (directory / "runtime.cpp").write_text(harness, encoding="utf-8")
        cls.executable = directory / ("runtime.exe" if os.name == "nt" else "runtime")
        command = [compiler, "-std=c++20", "-fno-exceptions", "-Wall", "-Wextra", "-Werror",
                   "-I", str(directory), "-I", str(json_include),
                   str(directory / "runtime.cpp"), str(directory / "I18n.cpp"),
                   str(directory / "I18nStrings.cpp"), "-o", str(cls.executable)]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.executable), case], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_registry_and_picker_indices(self):
        self.run_case("registry")

    def test_korean_and_english_fallback_strings(self):
        self.run_case("translations")

    def test_code_lookup_and_invalid_input(self):
        self.run_case("codes")

    def test_saved_language_and_repeated_load_fallback(self):
        self.run_case("load")

    def test_save_preserves_code_contract(self):
        self.run_case("save")


if __name__ == "__main__":
    unittest.main()
