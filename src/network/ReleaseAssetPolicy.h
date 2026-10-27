#pragma once

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
  for (const auto& entry : BOARDS) {
    if (entry.board == board) return entry.device;
  }
  return {};
}

}  // namespace release_asset_policy
