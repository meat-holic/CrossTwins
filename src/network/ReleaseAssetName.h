#pragma once

#include <cstddef>
#include <string_view>

#include "ReleaseAssetPolicy.h"
#include "ReleaseVersion.h"

namespace release_asset {

constexpr std::string_view deviceForBoard(std::string_view board) {
  return release_asset_policy::deviceForBoard(board);
}

inline bool buildFirmwareAssetName(std::string_view tag, std::string_view board, char* output, std::size_t capacity) {
  release_version::Version version;
  const auto normalized = release_version::normalizedTag(tag);
  if (!output || !release_version::parse(tag, version)) return false;
  const auto device = deviceForBoard(board);
  if (device.empty()) return false;

  constexpr auto prefix = release_asset_policy::PREFIX;
  const std::size_t required = prefix.size() + 1 + normalized.size() + 1 + device.size() + sizeof(".bin");
  if (capacity < required) return false;

  std::size_t offset = 0;
  const auto append = [&](std::string_view part) {
    for (const char character : part) output[offset++] = character;
  };
  append(prefix);
  output[offset++] = '-';
  append(normalized);
  output[offset++] = '-';
  append(device);
  append(".bin");
  output[offset] = '\0';
  return true;
}

}  // namespace release_asset
