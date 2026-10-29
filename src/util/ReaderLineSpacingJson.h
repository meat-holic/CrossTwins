#pragma once

#include <ArduinoJson.h>

#include <cstdint>

#include "util/ReaderLineSpacingMigration.h"

namespace reader_line_spacing {

enum class WebRequestState : uint8_t { MISSING, VALID, INVALID };

inline void writeJson(JsonDocument& doc, const uint8_t value) { doc["lineHeight"] = value; }

// Returns true when a legacy value was consumed and the settings document needs resaving.
inline bool readJson(const JsonVariantConst doc, uint8_t& value, const uint8_t optionCount,
                     const uint8_t defaultValue) {
  const auto current = doc["lineHeight"];
  if (!current.isUnbound()) {
    if (!current.is<uint8_t>()) {
      value = defaultValue;
      return false;
    }
    const uint8_t loaded = current.as<uint8_t>();
    value = loaded < optionCount ? loaded : defaultValue;
    return false;
  }

  const auto legacy = doc["lineSpacing"];
  if (!legacy.is<uint8_t>()) return false;

  value = migrateLegacyLineSpacing(legacy.as<uint8_t>(), optionCount, defaultValue);
  return true;
}

// Accept the previous web API key while preferring the current key when both are sent.
inline WebRequestState readWebRequest(const JsonVariantConst doc, uint8_t& value, const uint8_t optionCount) {
  const auto current = doc["lineHeight"];
  if (!current.isUnbound()) {
    if (!current.is<uint8_t>()) return WebRequestState::INVALID;
    const uint8_t loaded = current.as<uint8_t>();
    if (loaded >= optionCount) return WebRequestState::INVALID;
    value = loaded;
    return WebRequestState::VALID;
  }

  const auto legacy = doc["lineSpacing"];
  if (legacy.isUnbound()) return WebRequestState::MISSING;
  if (!legacy.is<uint8_t>() || optionCount < 2 || legacy.as<uint8_t>() >= optionCount - 1)
    return WebRequestState::INVALID;

  value = migrateLegacyLineSpacing(legacy.as<uint8_t>(), optionCount, 0);
  return WebRequestState::VALID;
}

}  // namespace reader_line_spacing
