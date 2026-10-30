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
    def test_exact_five_names_and_optional_v_prefix(self) -> None:
        version = "2.0.0-rc2.rev.2"
        expected = [f"crosstwins-{version}-{device}.bin" for device in ("x3-x4", "sticky", "x4pro", "x4c", "papermono")]
        self.assertEqual(release_assets.expected_names(f"v{version}"), expected)
        self.assertEqual(release_assets.expected_names(version), expected)
        self.assertEqual(release_assets.asset_name(f"v{version}", "x3-x4"), expected[0])
        self.assertEqual(release_assets.normalize_tag("v2.0.0-rc3"), "2.0.0-rc3")
        self.assertEqual(release_assets.normalize_tag("2.0.0-rc2.rev.3"), "2.0.0-rc2.rev.3")

    def test_rejects_unknown_device_and_malformed_version(self) -> None:
        for invalid in ("vv2.0.0", "2.0", "2.0.0-dev", "2.0.0-rc02"):
            with self.subTest(version=invalid), self.assertRaises(ValueError):
                release_assets.normalize_tag(invalid)
        with self.assertRaises(ValueError):
            release_assets.asset_name("2.0.0", "m5paper")

    def test_manifest_records_source_and_recomputed_sha256(self) -> None:
        version = "2.0.0-rc2.rev.2"
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

    def test_workflows_and_ota_use_shared_crosstwins_policy(self) -> None:
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
        self.assertIn("Refuse pre-existing release assets", release)
        self.assertIn("gh release view", release)
        self.assertIn(".assets | length", release)
        self.assertIn('test "$asset_count" = "0"', release)
        self.assertIn('test "$RELEASE_PRERELEASE" = "false"', release)
        self.assertIn('test "$(gh release view "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" --json isPrerelease --jq \'.isPrerelease\')" = "$RELEASE_PRERELEASE"', release)
        self.assertIn('python scripts/release_assets.py list --tag "$RELEASE_TAG"', release)
        release_build_step = release.split("      - name: Build CrossTwins\n", 1)[1].split("\n      - name:", 1)[0]
        self.assertIn("RELEASE_TAG: ${{ github.event.release.tag_name }}", release_build_step)
        self.assertIn('CROSSPOINT_RELEASE_VERSION="${RELEASE_TAG#v}" pio run', release_build_step)
        self.assertIn('configured_series="${configured_version%%-*}"', release)
        self.assertIn('CROSSPOINT_RELEASE_VERSION="$version" pio run', release_candidate)
        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertEqual(platformio.count("${sysenv.CROSSPOINT_RELEASE_VERSION}"), 8)
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
        match = re.search(r'^#define CROSSTWINS_VERSION_DISPLAY "([^"]+)"$', header, re.MULTILINE)

        self.assertIsNotNone(match, "Firmware display version is missing")
        self.assertEqual(match.group(1), f"CrossTwins {display_version}")


if __name__ == "__main__":
    unittest.main()
