#pragma once

#include <ArduinoJson.h>

namespace pluginevents::detail {

// Missing means unfiltered; an explicitly present value must be a string.
inline bool readRequiredMeta(const JsonVariantConst handler, const char*& key) {
  const JsonVariantConst value = handler["require_meta"];
  if (value.isUnbound()) {
    key = "";
    return true;
  }
  if (!value.is<const char*>()) return false;

  key = value.as<const char*>();
  return key != nullptr;
}

}  // namespace pluginevents::detail
