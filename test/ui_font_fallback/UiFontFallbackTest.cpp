#include "GfxRenderer/UiFontFallback.h"

#include <gtest/gtest.h>

#include <cstdint>

TEST(UiFontFallback, KeepsFullyCoveredMixedTextInSdFont) {
  constexpr int SD_FONT = 17;
  constexpr int BUILTIN_FONT = 23;
  const char text[] = "한글 UI 23";
  const int selected = uiFontFallback::chooseFontId(
      SD_FONT, BUILTIN_FONT, text, [](int fontId, uint32_t cp) { return fontId == SD_FONT || cp < 0x80; });
  EXPECT_EQ(selected, SD_FONT);
}

TEST(UiFontFallback, RoutesWholeStringToBuiltinWhenSdGlyphIsMissing) {
  constexpr int SD_FONT = 17;
  constexpr int BUILTIN_FONT = 23;
  const char text[] = "한글 UI 23";
  const int selected = uiFontFallback::chooseFontId(
      SD_FONT, BUILTIN_FONT, text, [](int fontId, uint32_t cp) { return fontId != SD_FONT || cp < 0xAC00; });
  EXPECT_EQ(selected, BUILTIN_FONT);
}

TEST(UiFontFallback, EmptyTextDoesNotRedirect) {
  EXPECT_EQ(uiFontFallback::chooseFontId(17, 23, "", [](int, uint32_t) { return false; }), 17);
}
