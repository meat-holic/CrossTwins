#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstring>

namespace ui_font_settings {

inline const char* legacyPath(JsonVariantConst settings, size_t familyCapacity, size_t pathCapacity) {
  const char* family = settings["uiFontFamilyName"].as<const char*>();
  // A saved selection still belongs to the user while its SD files are unavailable.
  // An empty string explicitly selects the built-in font.
  if (family && strlen(family) < familyCapacity) return nullptr;
  if ((settings["uiFontMode"] | 1) != 1) return nullptr;
  const char* path = settings["uiSdFontPath"].as<const char*>();
  return path && *path && strlen(path) < pathCapacity ? path : nullptr;
}

template <typename FindFamily>
const char* migrate(JsonDocument& settings, size_t familyCapacity, size_t pathCapacity, FindFamily findFamily) {
  const char* path = legacyPath(settings.as<JsonVariantConst>(), familyCapacity, pathCapacity);
  if (!path) return nullptr;
  const char* family = findFamily(path);
  if (!family || !*family || strlen(family) >= familyCapacity) return nullptr;
  return settings["uiFontFamilyName"].set(family) ? family : nullptr;
}

}  // namespace ui_font_settings
