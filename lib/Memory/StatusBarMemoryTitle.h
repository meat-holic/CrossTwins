#pragma once

#include <cstddef>
#include <cstdio>

namespace status_bar_memory {

inline constexpr size_t kTitleBufferSize = 64;

// Whole KiB values use floor division; zero is shown as 0K. A largest block
// greater than total free bytes is an invalid snapshot and uses the fallback.
inline bool formatTitle(char* output, const size_t outputSize, const size_t freeBytes,
                        const size_t largestFreeBlockBytes, const char* formatText, const char* unavailableText) {
  if (output == nullptr || outputSize == 0) return false;

  int written;
  if (largestFreeBlockBytes > freeBytes) {
    if (unavailableText == nullptr) {
      output[0] = '\0';
      return false;
    }
    written = snprintf(output, outputSize, "%s", unavailableText);
  } else {
    if (formatText == nullptr) {
      output[0] = '\0';
      return false;
    }
    written = snprintf(output, outputSize, formatText, static_cast<unsigned long long>(freeBytes / 1024),
                       static_cast<unsigned long long>(largestFreeBlockBytes / 1024));
  }

  if (written < 0 || static_cast<size_t>(written) >= outputSize) {
    output[outputSize - 1] = '\0';
    return false;
  }
  return true;
}

}  // namespace status_bar_memory
