#!/usr/bin/env python3
"""Generate the small C++ OTA asset policy from the shared JSON source."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "config" / "release_assets.json"
OUTPUT = ROOT / "src" / "network" / "ReleaseAssetPolicy.h"


def render() -> str:
    policy = json.loads(SOURCE.read_text(encoding="utf-8"))
    repository = policy["repository"]
    prefix = policy["prefix"]
    boards = policy["boards"]
    if not repository or not prefix or not boards:
        raise ValueError("release asset policy must define a repository, prefix, and boards")
    if any(not entry["board"] or not entry["device"] for entry in boards):
        raise ValueError("board and device names must be non-empty")
    if len({entry["board"] for entry in boards}) != len(boards):
        raise ValueError("board names must be unique")
    if any(not name.replace("-", "").isalnum() or name.lower() != name for name in (prefix, *[e["board"] for e in boards], *[e["device"] for e in boards])):
        raise ValueError("asset policy names must use lowercase ASCII letters, digits, and hyphens")

    rows = "\n".join(f'    {{"{entry["board"]}", "{entry["device"]}"}},' for entry in boards)
    return f'''#pragma once

#include <array>
#include <string_view>

namespace release_asset_policy {{

inline constexpr std::string_view REPOSITORY = "{repository}";
inline constexpr std::string_view PREFIX = "{prefix}";

struct BoardAsset {{
  std::string_view board;
  std::string_view device;
}};

inline constexpr std::array<BoardAsset, {len(boards)}> BOARDS{{{{
{rows}
}}}};

constexpr std::string_view deviceForBoard(std::string_view board) {{
  for (const auto& entry : BOARDS) {{
    if (entry.board == board) return entry.device;
  }}
  return {{}};
}}

}}  // namespace release_asset_policy
'''


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if the generated header is stale")
    args = parser.parse_args()
    expected = render()
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != expected:
            print(f"{OUTPUT.relative_to(ROOT)} is stale; run {Path(__file__).name}")
            return 1
        print("Release asset policy header matches config/release_assets.json")
        return 0
    OUTPUT.write_text(expected, encoding="utf-8", newline="\n")
    print(f"Wrote {OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
