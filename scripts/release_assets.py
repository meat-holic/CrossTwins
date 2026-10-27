#!/usr/bin/env python3
"""Create and verify CrossTwins firmware asset names and release manifests."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / "config" / "release_assets.json").read_text(encoding="utf-8"))
VERSION_RE = re.compile(
    r"v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-rc(0|[1-9][0-9]*)(?:\.rev\.(0|[1-9][0-9]*))?)?\Z"
)
MAX_COMPONENT = (1 << 32) - 1
DEVICES = tuple(dict.fromkeys(entry["device"] for entry in POLICY["boards"]))


def normalize_tag(tag: str) -> str:
    match = VERSION_RE.fullmatch(tag)
    if not match:
        raise ValueError(f"invalid release tag: {tag!r}")
    if any(int(component) > MAX_COMPONENT for component in match.groups() if component is not None):
        raise ValueError(f"release version component out of range: {tag!r}")
    return tag[1:] if tag.startswith("v") else tag


def asset_name(tag: str, device: str) -> str:
    version = normalize_tag(tag)
    if device not in DEVICES:
        raise ValueError(f"unsupported release device: {device!r}")
    return f"{POLICY['prefix']}-{version}-{device}.bin"


def expected_names(tag: str) -> list[str]:
    version = normalize_tag(tag)
    return [f"{POLICY['prefix']}-{version}-{device}.bin" for device in DEVICES]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_asset_directory(tag: str, directory: Path) -> list[Path]:
    expected = expected_names(tag)
    actual = sorted(path.name for path in directory.glob("*.bin"))
    if actual != sorted(expected):
        raise ValueError(f"firmware assets mismatch: expected {sorted(expected)}, found {actual}")
    return [directory / name for name in expected]


def manifest(tag: str, commit: str, directory: Path) -> dict[str, object]:
    version = normalize_tag(tag)
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("source commit must be a full 40-character Git SHA")
    assets = validate_asset_directory(tag, directory)
    return {
        "context": {
            "repository": POLICY["repository"],
            "commit_sha": commit.lower(),
            "tag": tag,
            "version": version,
        },
        "artifacts": [
            {
                "asset_name": path.name,
                "sha256": sha256(path),
                "size": path.stat().st_size,
                "device": device,
            }
            for device, path in zip(DEVICES, assets)
        ],
    }


def verify_manifest(tag: str, commit: str, directory: Path, manifest_path: Path) -> None:
    actual = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected = manifest(tag, commit, directory)
    if actual != expected:
        raise ValueError("release manifest does not match the downloaded firmware assets and source commit")


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    name_parser = subparsers.add_parser("name")
    name_parser.add_argument("--tag", required=True)
    name_parser.add_argument("--device", required=True)

    list_parser = subparsers.add_parser("list")
    list_parser.add_argument("--tag", required=True)

    manifest_parser = subparsers.add_parser("manifest")
    manifest_parser.add_argument("--tag", required=True)
    manifest_parser.add_argument("--commit", required=True)
    manifest_parser.add_argument("--assets-dir", type=Path, required=True)
    manifest_parser.add_argument("--output", type=Path, required=True)

    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--tag", required=True)
    verify_parser.add_argument("--commit", required=True)
    verify_parser.add_argument("--assets-dir", type=Path, required=True)
    verify_parser.add_argument("--manifest", type=Path, required=True)

    args = parser.parse_args()
    try:
        if args.command == "name":
            print(asset_name(args.tag, args.device))
        elif args.command == "list":
            print("\n".join(expected_names(args.tag)))
        elif args.command == "manifest":
            result = manifest(args.tag, args.commit, args.assets_dir)
            args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
            print(args.output)
        else:
            verify_manifest(args.tag, args.commit, args.assets_dir, args.manifest)
            print("Firmware assets and manifest are valid")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"release asset validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
