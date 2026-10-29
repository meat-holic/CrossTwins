#pragma once

#include <cstdint>

// The 100% choice takes slot 0; keep every previously saved spacing in the same slot by shifting it once.
constexpr uint8_t migrateLegacyLineSpacing(const uint8_t legacyValue, const uint8_t optionCount,
                                           const uint8_t defaultValue) {
  const uint16_t migratedValue = static_cast<uint16_t>(legacyValue) + 1;
  return migratedValue < optionCount ? static_cast<uint8_t>(migratedValue) : defaultValue;
}
