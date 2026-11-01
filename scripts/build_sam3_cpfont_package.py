#!/usr/bin/env python3
"""Build the release's optional Sam3KRFont-derived CPFont archive from its original TTF."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


FONT_PREFIX = "Sam3KRFont"
FONT_FILES = tuple(f"{FONT_PREFIX}_{size}.cpfont" for size in range(10, 14))
INTERVALS = "ascii,hangul,cjk,symbols"
SOURCE_URL = "https://github.com/hurss/fonts"


def build_package(ttf: Path, license_file: Path, converter: Path, output: Path,
                  source_revision: str) -> None:
    if not ttf.is_file() or ttf.name != "Sam3KRFont.ttf":
        raise ValueError("expected the original Sam3KRFont.ttf source file")
    if not license_file.is_file():
        raise ValueError("the original font LICENSE.txt is required")
    if output.exists():
        raise ValueError(f"refusing to overwrite existing package: {output}")

    with tempfile.TemporaryDirectory(prefix="sam3-cpfont-") as temp:
        temp_root = Path(temp)
        package = temp_root / FONT_PREFIX
        package.mkdir()
        shutil.copyfile(license_file, package / "LICENSE.txt")
        readme = f"""Sam3KRFont CPFont package derived from the original TTF

Original font: Sam3KRFont (Korean Three Kingdoms III style bitmap font)
Original author: Damheo Lee (이담허 / leedheo)
Source repository: {SOURCE_URL}
Source revision: {source_revision}
Source file: ttf/Sam3KRFont.ttf

These CPFont files were generated directly from the original TTF with
CrossTwins' lib/EpdFont/scripts/fontconvert_sdcard.py. The TTF was not included.
The complete, unmodified original license is included, including its Reserved
Font Name statement.

Included files:
{chr(10).join(f'- {name}' for name in FONT_FILES)}

Installation:
Copy the Sam3KRFont folder into the SD card's /.fonts directory. In CrossTwins,
enable SD UI Font and select the desired file in Settings > Display > UI Font
Selection. UI and Reader font selections are independent.

License: MIT. See LICENSE.txt for the original copyright, full terms, and
Reserved Font Name statement.
"""
        (package / "README.txt").write_text(readme, encoding="utf-8", newline="\n")

        subprocess.run(
            [sys.executable, str(converter), str(ttf), "--intervals", INTERVALS,
             "--sizes", "10,11,12,13", "--name", FONT_PREFIX,
             "--output-dir", str(package)],
            check=True,
        )

        sys.path.insert(0, str(converter.parent))
        from cpfont_version import CPFONT_VERSION  # pylint: disable=import-outside-toplevel

        expected = {"LICENSE.txt", "README.txt", *FONT_FILES}
        actual = {path.name for path in package.iterdir() if path.is_file()}
        if actual != expected:
            raise ValueError(f"unexpected package files: {sorted(actual)}")

        for name in FONT_FILES:
            data = (package / name).read_bytes()
            if len(data) < 64:
                raise ValueError(f"truncated CPFont file: {name}")
            magic, version, _flags, style_count = struct.unpack_from("<8sHHB", data)
            if magic != b"CPFONT\0\0" or version != CPFONT_VERSION or style_count != 1:
                raise ValueError(f"invalid or unsupported CPFont header: {name}")

        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(package.iterdir()):
                info = zipfile.ZipInfo(f"{FONT_PREFIX}/{path.name}", date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, path.read_bytes())

    with zipfile.ZipFile(output) as archive:
        expected_entries = {f"{FONT_PREFIX}/{name}" for name in expected}
        if set(archive.namelist()) != expected_entries or archive.testzip() is not None:
            output.unlink(missing_ok=True)
            raise ValueError("generated ZIP failed its integrity or contents check")

    print(f"Created {output} ({output.stat().st_size} bytes)")
    print(f"SHA-256: {hashlib.sha256(output.read_bytes()).hexdigest()}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ttf", type=Path, required=True)
    parser.add_argument("--license", type=Path, required=True)
    parser.add_argument("--converter", type=Path, required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        build_package(args.ttf, args.license, args.converter, args.output, args.source_revision)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"CPFont package generation failed: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
