#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace minimal_home_settings {
inline constexpr const char* kAlignmentKey = "minimalMenuAlignment";
inline constexpr const char* kBackgroundPathKey = "minimalHomeBackgroundPath";
inline constexpr uint8_t kAlignmentCount = 3;
inline constexpr uint8_t kDefaultAlignment = 0;

inline void writeJson(JsonDocument& document, const uint8_t alignment, const char* backgroundPath) {
  document[kAlignmentKey] = alignment;
  document[kBackgroundPathKey] = backgroundPath ? backgroundPath : "";
}

inline bool readJson(const JsonVariantConst document, uint8_t& alignment, char* backgroundPath,
                     const size_t backgroundPathSize) {
  bool needsResave = false;
  const uint8_t storedAlignment = document[kAlignmentKey] | alignment;
  alignment = storedAlignment < kAlignmentCount ? storedAlignment : kDefaultAlignment;
  if (storedAlignment != alignment) needsResave = true;

  if (backgroundPath && backgroundPathSize > 0 && document[kBackgroundPathKey].is<const char*>()) {
    const char* storedPath = document[kBackgroundPathKey].as<const char*>();
    size_t length = 0;
    while (storedPath && length < backgroundPathSize && storedPath[length] != '\0') ++length;
    if (!storedPath || length >= backgroundPathSize) {
      backgroundPath[0] = '\0';
      needsResave = true;
    } else {
      std::memcpy(backgroundPath, storedPath, length + 1);
    }
  }
  return needsResave;
}
}  // namespace minimal_home_settings
