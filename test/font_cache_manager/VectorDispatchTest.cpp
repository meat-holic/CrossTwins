#include <gtest/gtest.h>

#include "FontCacheManager.h"
#include "FontDecompressor.h"
#include "SdCardFont.h"
#include "TtfEpdFont.h"
TEST(FontCacheVector, KeepsTtfAndCpfontDispatchDistinctAcrossPressureRelease) {
  std::map<int, EpdFontFamily> fonts;
  SdCardFont sd;
  TtfEpdFont ttf;
  FontDecompressor decompressor;
  std::map<int, SdCardFont*> sdFonts{{7, &sd}};
  std::map<int, TtfEpdFont*> ttfFonts{{9, &ttf}};
  FontCacheManager manager(fonts, sdFonts, ttfFonts);
  manager.setFontDecompressor(&decompressor);
  manager.prewarmCache(9, "first", 1, false);
  manager.prewarmCache(9, "more", 1, true);
  manager.prewarmCache(7, "sd", 1, false);
  EXPECT_EQ(ttf.buildCalls, 1);
  EXPECT_EQ(ttf.addCalls, 1);
  EXPECT_EQ(sd.prewarmCallCount, 1);
  for (int i = 0; i < 3; ++i) {
    manager.clearCache();
    manager.releaseSdFontCaches();
  }
  EXPECT_EQ(ttf.clearCalls, 3);
  EXPECT_EQ(ttf.releaseCalls, 3);
  EXPECT_EQ(sd.clearCalls, 3);
  EXPECT_EQ(sd.releaseCalls, 3);
  EXPECT_EQ(decompressor.clearCalls, 6);
  manager.prewarmCache(9, "after", 1, false);
  EXPECT_EQ(ttf.buildCalls, 2);
  EXPECT_STREQ(ttf.lastText, "after");
}
