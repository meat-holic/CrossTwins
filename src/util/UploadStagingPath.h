#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace upload_staging {

constexpr size_t MAX_PATH_ATTEMPTS = 8;
constexpr size_t MAX_SUFFIX_LENGTH = sizeof(".upload-00000000.part") - 1;

// Select an unused sibling file for an upload that will replace destination.
// The caller supplies a per-upload random seed and an existence check; a bounded
// retry keeps a collision from truncating another file via an O_TRUNC open.
template <typename Exists>
bool selectPath(const char* destination, const uint32_t seed, Exists&& exists, char* selected,
                const size_t selectedCapacity) {
  if (!selected || selectedCapacity == 0) return false;
  selected[0] = '\0';
  if (!destination) return false;
  const size_t destinationLength = std::strlen(destination);
  for (size_t attempt = 0; attempt < MAX_PATH_ATTEMPTS; ++attempt) {
    char suffix[32];
    const uint32_t token = seed + static_cast<uint32_t>(attempt);
    const int length = std::snprintf(suffix, sizeof(suffix), ".upload-%08lx.part", static_cast<unsigned long>(token));
    if (length < 0 || static_cast<size_t>(length) >= sizeof(suffix)) break;
    if (destinationLength + static_cast<size_t>(length) + 1 > selectedCapacity) return false;

    std::memcpy(selected, destination, destinationLength);
    std::memcpy(selected + destinationLength, suffix, static_cast<size_t>(length) + 1);
    if (!exists(selected)) return true;
  }

  selected[0] = '\0';
  return false;
}

}  // namespace upload_staging
