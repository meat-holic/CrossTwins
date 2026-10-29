import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "patch_wolfssl.py"


class FakeEnvironment:
    def __init__(self, project_dir: Path):
        self.project_dir = project_dir

    def subst(self, value: str) -> str:
        if value != "$PROJECT_DIR":
            raise AssertionError(f"unexpected SCons substitution: {value}")
        return str(self.project_dir)


def load_patch_function(project_dir: Path):
    namespace = {}

    def import_scons_value(name: str) -> None:
        if name != "env":
            raise AssertionError(f"unexpected SCons import: {name}")
        namespace["env"] = FakeEnvironment(project_dir)

    namespace["Import"] = import_scons_value
    source = SCRIPT.read_text(encoding="utf-8")
    exec(compile(source, str(SCRIPT), "exec"), namespace)
    return namespace["patch_user_settings"], namespace["OVERRIDES"]


class WolfSslPatchTest(unittest.TestCase):
    def test_patch_is_idempotent_and_preserves_debug_build_guard(self):
        with tempfile.TemporaryDirectory() as directory:
            project_dir = Path(directory)
            patch_user_settings, overrides = load_patch_function(project_dir)
            settings = project_dir / "user_settings.h"
            settings.write_text("#define WOLFSSL_USER_SETTINGS\n#define DEBUG_WOLFSSL\n", encoding="utf-8")

            patch_user_settings(settings)
            once = settings.read_text(encoding="utf-8")
            patch_user_settings(settings)
            twice = settings.read_text(encoding="utf-8")

            self.assertEqual(once, twice)
            self.assertEqual(twice.count("CrossPoint wolfSSL compatibility overrides"), 1)
            self.assertIn("#ifndef FREEINK_WOLFSSL_DEBUG\n#undef DEBUG_WOLFSSL\n#endif", overrides)
            self.assertTrue(twice.startswith("#define WOLFSSL_USER_SETTINGS\n"))

            compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            if compiler is None:
                self.skipTest("no host C preprocessor is available")

            probe = project_dir / "debug_macro_probe.c"
            probe.write_text(
                '#include "user_settings.h"\n'
                "#ifdef FREEINK_WOLFSSL_DEBUG\n"
                "#ifndef DEBUG_WOLFSSL\n#error debug build lost wolfSSL trace\n#endif\n"
                "#else\n"
                "#ifdef DEBUG_WOLFSSL\n#error ordinary build retained wolfSSL trace\n#endif\n"
                "#endif\n"
                "int main(void) { return 0; }\n",
                encoding="utf-8",
            )
            base = [compiler, "-fsyntax-only", "-I", str(project_dir), str(probe)]
            subprocess.run(base, check=True, capture_output=True, text=True)
            subprocess.run(base[:1] + ["-DFREEINK_WOLFSSL_DEBUG"] + base[1:], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
