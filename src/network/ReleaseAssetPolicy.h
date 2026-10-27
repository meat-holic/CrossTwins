#pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace release_asset_policy {

inline constexpr std::string_view REPOSITORY = "meat-holic/CrossTwins";
inline constexpr std::string_view PREFIX = "crosstwins";

struct BoardAsset {
  std::string_view board;
  std::string_view device;
};

inline constexpr std::array<BoardAsset, 6> BOARDS{{
    {"x3", "x3-x4"},
    {"x4", "x3-x4"},
    {"sticky", "sticky"},
    {"x4pro", "x4pro"},
    {"x4c", "x4c"},
    {"papermono", "papermono"},
}};

constexpr std::string_view deviceForBoard(std::string_view board) {
  const auto entry =
      std::find_if(BOARDS.begin(), BOARDS.end(), [board](const BoardAsset& asset) { return asset.board == board; });
  return entry == BOARDS.end() ? std::string_view{} : entry->device;
}

}  // namespace release_asset_policy
