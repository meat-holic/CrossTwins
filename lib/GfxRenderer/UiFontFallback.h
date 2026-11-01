#pragma once

#include <Utf8.h>

#include <cstdint>

namespace uiFontFallback {

template <typename SupportsCodepoint>
int chooseFontId(const int preferredFontId, const int fallbackFontId, const char* text,
                 SupportsCodepoint supportsCodepoint) {
  if (text == nullptr || *text == '\0') return preferredFontId;

  const auto* cursor = reinterpret_cast<const uint8_t*>(text);
  uint32_t codepoint;
  while ((codepoint = utf8NextCodepoint(&cursor))) {
    if (!supportsCodepoint(preferredFontId, codepoint)) return fallbackFontId;
  }
  return preferredFontId;
}

}  // namespace uiFontFallback
