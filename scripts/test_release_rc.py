"""RC identity contracts; no firmware build, network, tag or publication."""
import json
import subprocess
import sys
import unittest
import importlib.util
import contextlib
import io
import os
from unittest.mock import patch
from pathlib import Path

import release_version as rv
from release_preflight import REPOSITORY, SDK_SHA, context, validate_context

ROOT = Path(__file__).resolve().parents[1]
SHA = "a" * 40


class CrossTwinsRcContract(unittest.TestCase):
    def test_rc_zero_has_canonical_semver_assets_and_display(self):
        release = rv.resolve_release("v2.0.0-rc.ct.0", "2.0.0", True)
        self.assertEqual(release.asset_version, "2.0.0-rc.ct.0")
        self.assertEqual(release.firmware_suffix, "-rc.ct.0")
        self.assertEqual(rv.firmware_version(release, "2.0.0", SHA[:7]), "2.0.0-rc.ct.0+aaaaaaa")
        self.assertEqual(rv.display_version(release, "2.0.0"), "CrossTwins-2.0.0RC-ct.0")
        self.assertLess(len(rv.firmware_version(release, "2.0.0", SHA[:7])), 32)

    def test_rc_requires_prerelease_and_preserves_stable_counter_rules(self):
        for tag, pre in [("v2.0.0-rc.ct.0", False), ("v2.0.0-rc.ct.00", True),
                         ("v2.0.0-rc.ct.-1", True), ("v2.0.0-ct.0", True)]:
            with self.subTest(tag=tag, pre=pre), self.assertRaises(ValueError):
                rv.resolve_release(tag, "2.0.0", pre)
        self.assertEqual(rv.resolve_release("v1.6.5-ct.7", "1.6.5", False).firmware_suffix, "-ct.7")

    def test_rc_preflight_separates_schema_without_weakening_stable(self):
        value = context(REPOSITORY, "v2.0.0-rc.ct.0", SHA, SHA, SDK_SHA, "2.0.0", "1", "1", True, prerelease=True)
        validate_context(value)
        self.assertEqual(value["schema_version"], 2)
        self.assertIs(value["prerelease"], True)
        self.assertEqual(value["mode"], "local")
        stable = context(REPOSITORY, "v1.6.5-ct.7", SHA, SHA, SDK_SHA, "1.6.5", "1", "1")
        self.assertEqual(stable["schema_version"], 1)
        for kwargs in ({}, {"prerelease": False}, {"prerelease": 1}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                context(REPOSITORY, "v2.0.0-rc.ct.0", SHA, SHA, SDK_SHA, "2.0.0", "1", "1", **kwargs)
        with self.assertRaises(ValueError):
            context(REPOSITORY, "v1.6.5-ct.2", SHA, SHA, SDK_SHA, "1.6.5", "1", "1")

    def test_rc_context_cannot_change_identity_or_publication_status(self):
        value = context(REPOSITORY, "v2.0.0-rc.ct.0", SHA, SHA, SDK_SHA, "2.0.0", "1", "1", prerelease=True)
        for key, replacement in [("schema_version", 1), ("prerelease", False), ("sdk_gitlink_sha", "b" * 40)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_context(dict(value, **{key: replacement}))

    def test_publication_resolver_keeps_legacy_and_rc_separate(self):
        for tag, base, expected in [("v2.0.0-rc.ct.0", "2.0.0", True),
                                    ("v1.6.5-ct.7", "1.6.5", False),
                                    ("v1.6.5-ct.7-aaaaaaa", "1.6.5", True)]:
            value = rv.resolve_publication(tag, base, SHA[:7])
            self.assertEqual(value["prerelease"], expected)
            self.assertEqual(value["sha"], SHA[:7])
        for tag in ["v2.0.0-ct.0", "v2.0.0-rc.ct.01", "v2.0.0-ct.7-bbbbbbb", "v1.6.5-ct.7"]:
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                rv.resolve_publication(tag, "2.0.0", SHA[:7])

    def test_rc_official_asset_set_stays_fourteen_including_checksums(self):
        release = rv.resolve_release("v2.0.0-rc.ct.0", "2.0.0", True)
        names = {rv.asset_name(release, device) for device in rv.DEVICES}
        names.update(f"CrossPoint-v2.0.0-rc.ct.0-{device}-full.bin" for device in rv.DEVICES)
        names.update(("firmware.bin", "bootloader.bin", "partitions.bin", "SHA256SUMS"))
        self.assertEqual(len(names), 14)
        self.assertEqual(len({name.lower() for name in names}), 14)
        self.assertFalse(any("ble_off" in name or "debug" in name for name in names))

    def test_rc_publication_waits_in_draft_and_cannot_become_latest(self):
        workflow = (ROOT / ".github/workflows/release-publish.yml").read_text(encoding="utf-8")
        self.assertIn("draft: ${{ needs.version.outputs.prerelease == 'true' }}", workflow)
        self.assertIn("prerelease: ${{ needs.version.outputs.prerelease == 'true' }}", workflow)
        self.assertIn("make_latest: ${{ needs.version.outputs.prerelease == 'true' && 'false' || 'true' }}", workflow)
        ci = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIn("branches: [master, experiment/upstream-2.0.0rc]", ci)

    def test_publication_cli_returns_exact_safe_outputs(self):
        command = [sys.executable, str(ROOT / "scripts/release_version.py"), "--tag", "v2.0.0-rc.ct.0",
                   "--base-version", "2.0.0", "--publication-sha", SHA[:7]]
        result = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        outputs = dict(line.split("=", 1) for line in result.stdout.splitlines())
        self.assertEqual(outputs["prerelease"], "true")
        self.assertEqual(outputs["suffix"], "-rc.ct.0")
        self.assertEqual(outputs["sha"], SHA[:7])

    def test_rc_build_label_and_sha_guard_use_the_same_contract(self):
        spec = importlib.util.spec_from_file_location("rc_git_branch", ROOT / "scripts/git_branch.py")
        module = importlib.util.module_from_spec(spec)
        with contextlib.redirect_stdout(io.StringIO()):
            spec.loader.exec_module(module)

        class Env(dict):
            definitions = None
            def Append(self, **values):
                self.definitions = values["CPPDEFINES"]

        env = Env(PIOENV="x4pro-gh_release_rc", PROJECT_DIR=str(ROOT))
        with patch.object(module, "get_git_short_sha", return_value=SHA[:7]), \
             patch.object(module, "get_base_version", return_value="2.0.0"), \
             patch.dict(os.environ, CROSSPOINT_RC_SUFFIX="-rc.ct.0", CROSSPOINT_RC_HASH=SHA[:7]):
            module.inject_version(env)
            self.assertEqual(env.definitions[0][0], "USER_VISIBLE_FIRMWARE_VERSION")
            self.assertIn("CrossTwins-2.0.0RC-ct.0", env.definitions[0][1])
            with patch.dict(os.environ, CROSSPOINT_RC_HASH="bbbbbbb"), self.assertRaises(ValueError):
                module.inject_version(env)


if __name__ == "__main__":
    unittest.main()
