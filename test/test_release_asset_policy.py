from __future__ import annotations

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import release_assets


class ReleaseAssetPolicyTests(unittest.TestCase):
    def test_exact_five_names_and_supported_version_formats(self) -> None:
        devices = ("x3-x4", "sticky", "x4pro", "x4c", "papermono")
        for version in ("2.0.0", "2.0.0-rc3", "2.0.0-rc2.rev.2", "2.0.0-rev.1"):
            with self.subTest(version=version):
                expected = [f"crosstwins-{version}-{device}.bin" for device in devices]
                self.assertEqual(release_assets.expected_names(f"v{version}"), expected)
                self.assertEqual(release_assets.expected_names(version), expected)
                self.assertEqual(release_assets.asset_name(f"v{version}", "x3-x4"), expected[0])
        self.assertEqual(release_assets.normalize_tag("v2.0.0-rc3"), "2.0.0-rc3")
        self.assertEqual(release_assets.normalize_tag("v2.0.0-rev.1"), "2.0.0-rev.1")

    def test_release_tag_validation_matches_stable_and_rc_metadata(self) -> None:
        self.assertEqual(release_assets.validate_release_tag("v2.0.0", "2.0.0", False), "2.0.0")
        self.assertEqual(
            release_assets.validate_release_tag("v2.0.0-rc2.rev.1", "2.0.0", True), "2.0.0-rc2.rev.1"
        )
        self.assertEqual(
            release_assets.validate_release_tag("v2.0.0-rev.1", "2.0.0-rev.1", False), "2.0.0-rev.1"
        )
        self.assertEqual(
            release_assets.validate_release_tag("v2.0.0-rc2.rev.2", "2.0.0-rev.1", False), "2.0.0-rc2.rev.2"
        )
        invalid_pairs = (
            ("2.0.0-rev.1", "2.0.0", False),
            ("2.0.0", "2.0.0-rev.1", True),
            ("2.1.0-rc1", "2.0.0-rev.1", True),
            ("2.0.0-rc3", "2.0.0-rev.1", False),
            ("2.0.0-rc2.rev.2", "2.0.0-rev.1", True),
        )
        for tag, configured, prerelease in invalid_pairs:
            with self.subTest(tag=tag, configured=configured, prerelease=prerelease), self.assertRaises(ValueError):
                release_assets.validate_release_tag(tag, configured, prerelease)

    def test_rejects_unknown_device_and_malformed_version(self) -> None:
        for invalid in ("vv2.0.0", "2.0", "2.0.0-dev", "2.0.0-rc02", "2.0.0-rev.01"):
            with self.subTest(version=invalid), self.assertRaises(ValueError):
                release_assets.normalize_tag(invalid)
        with self.assertRaises(ValueError):
            release_assets.asset_name("2.0.0", "m5paper")

    def test_manifest_records_source_and_recomputed_sha256(self) -> None:
        version = "2.0.0-rev.1"
        commit = "a" * 40
        with tempfile.TemporaryDirectory() as temporary:
            assets_dir = Path(temporary)
            for index, name in enumerate(release_assets.expected_names(version)):
                (assets_dir / name).write_bytes(f"firmware-{index}".encode())
            created = release_assets.manifest(f"v{version}", commit, assets_dir)
            manifest_path = assets_dir / "manifest.json"
            manifest_path.write_text(json.dumps(created), encoding="utf-8")
            release_assets.verify_manifest(f"v{version}", commit, assets_dir, manifest_path)
            self.assertEqual(created["context"]["commit_sha"], commit)
            self.assertEqual(len(created["artifacts"]), 5)
            self.assertTrue(all(len(asset["sha256"]) == 64 for asset in created["artifacts"]))
            self.assertEqual({asset["device"] for asset in created["artifacts"]}, set(release_assets.DEVICES))

    def test_workflow_validates_and_verifies_draft_without_publish_trigger(self) -> None:
        policy = json.loads((ROOT / "config" / "release_assets.json").read_text(encoding="utf-8"))
        ota = (ROOT / "src" / "network" / "OtaUpdater.cpp").read_text(encoding="utf-8")
        ci = (ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        release = (ROOT / ".github" / "workflows" / "release.yml").read_text(encoding="utf-8")
        release_candidate = (ROOT / ".github" / "workflows" / "release_candidate.yml").read_text(encoding="utf-8")
        self.assertIn(f"https://api.github.com/repos/{policy['repository']}/releases/latest", ota)
        self.assertNotIn("crosspoint-reader/crosspoint-reader/releases/latest", ota)
        self.assertIn("scripts/release_assets.py", ci)
        self.assertIn("scripts/release_assets.py", release)
        self.assertNotIn("crosspoint-*.bin", ci + release)
        self.assertNotIn("--clobber", release)
        self.assertIn("workflow_dispatch:", release)
        self.assertNotIn("types: [published]", release)
        self.assertIn("python scripts/release_assets.py validate", release)
        self.assertIn('test "$tag_commit" = "$workflow_commit"', release)
        self.assertIn('test "$(git rev-parse HEAD)" = "$EXPECTED_COMMIT"', release)
        self.assertIn("uploads.github.com/repos/${GITHUB_REPOSITORY}/releases/${RELEASE_ID}/assets", release)
        self.assertIn("Verify uploaded assets before publication", release)
        self.assertIn("verify-release:", release)
        self.assertNotIn("publish-release:", release)
        self.assertLess(
            release.index("Attach firmware assets and manifest to draft"),
            release.index("Verify uploaded assets before publication"),
        )
        self.assertNotIn("-F draft=false", release)
        self.assertNotIn("--draft=false", release)
        self.assertIn('CROSSPOINT_RELEASE_VERSION="$RELEASE_VERSION" PLATFORMIO_BUILD_FLAGS="$build_flags"', release)
        self.assertIn('pio run -e "$PIO_ENV" -j1', release)
        self.assertIn("-DCROSSTWINS_USE_BUILD_VERSION=1", release)
        self.assertIn('"Accept: application/octet-stream"', release)
        self.assertIn("--assets-dir verify", release)
        self.assertIn('python scripts/release_assets.py list --tag "$RELEASE_TAG"', release)
        self.assertIn('CROSSPOINT_RELEASE_VERSION="$version" PLATFORMIO_BUILD_FLAGS=', release_candidate)
        self.assertIn('pio run -e ${{ matrix.pio_env }} -j1', release_candidate)
        self.assertIn('PLATFORMIO_BUILD_FLAGS="-DCROSSTWINS_USE_BUILD_VERSION=1"', release_candidate)

        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertEqual(platformio.count("${sysenv.CROSSPOINT_RELEASE_VERSION}"), 8)
        configured_version = next(line for line in platformio.splitlines() if line.startswith("version = ")).split("=", 1)[1].strip()
        self.assertEqual(configured_version, "2.0.0-rev.1")
        self.assertEqual(
            release_assets.validate_release_tag("v2.0.0-rev.1", configured_version, False), configured_version
        )
        header = (ROOT / "src" / "network" / "ReleaseAssetPolicy.h").read_text(encoding="utf-8")
        self.assertIn(f'PREFIX = "{policy["prefix"]}"', header)
        for entry in policy["boards"]:
            self.assertIn(f'{{"{entry["board"]}", "{entry["device"]}"}}', header)

    def test_boot_and_sleep_screens_draw_the_generated_logo(self) -> None:
        for source in (
            ROOT / "src/activities/boot_sleep/BootActivity.cpp",
            ROOT / "src/activities/boot_sleep/SleepActivity.cpp",
        ):
            contents = source.read_text(encoding="utf-8")
            with self.subTest(source=source.name):
                self.assertIn('#include "images/Logo120.h"', contents)
                self.assertIn("renderer.drawImage(Logo120,", contents)
                self.assertIn("tr(STR_CROSSTWINS)", contents)

    def test_boot_brand_has_english_and_korean_entries(self) -> None:
        for language in ("english", "korean"):
            contents = (ROOT / "lib/I18n/translations" / f"{language}.yaml").read_text(encoding="utf-8")
            self.assertIn('STR_CROSSTWINS: "CrossTwins"', contents)

    def test_firmware_display_version_matches_configured_release(self) -> None:
        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        version_line = next(line for line in platformio.splitlines() if line.startswith("version = "))
        configured_version = version_line.removeprefix("version = ").strip()
        display_version = re.sub(r"-rc([0-9]+)(?:\.rev\.[0-9]+)?$", r"-RC\1", configured_version)
        header = (ROOT / "src/FirmwareVersion.h").read_text(encoding="utf-8")
        self.assertIn("#if defined(CROSSTWINS_USE_BUILD_VERSION) && defined(CROSSPOINT_VERSION)", header)
        self.assertIn('#define CROSSTWINS_VERSION_DISPLAY "CrossTwins " CROSSPOINT_VERSION', header)
        match = re.search(r'^#define CROSSTWINS_VERSION_DISPLAY "([^"]+)"$', header, re.MULTILINE)

        self.assertIsNotNone(match, "Firmware display version is missing")
        self.assertEqual(match.group(1), f"CrossTwins {display_version}")


if __name__ == "__main__":
    unittest.main()
