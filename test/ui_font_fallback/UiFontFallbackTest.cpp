#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "GfxRenderer/UiFontFallback.h"
#include "UiFontSelection.h"
#include "components/OptionPopupPaging.h"

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

TEST(UiFontSelection, OffersEveryRasterizedCpfontRegardlessOfFilenameSize) {
  SdCardFontFamilyInfo family;
  family.name = "Sam3KRFont";
  family.vector = false;
  family.files.emplace_back();
  family.files.back().pointSize = 8;
  EXPECT_TRUE(uiFontSelection::isSelectableCpfont(family, family.files.back()));
  family.files.emplace_back();
  family.files.back().pointSize = 23;
  EXPECT_TRUE(uiFontSelection::isSelectableCpfont(family, family.files.back()));
  family.files.emplace_back();
  family.files.back().pointSize = 0;
  EXPECT_FALSE(uiFontSelection::isSelectableCpfont(family, family.files.back()));
  family.vector = true;
  EXPECT_FALSE(uiFontSelection::isSelectableCpfont(family, family.files.front()));
}

TEST(UiFontSelection, RejectsMetricsThatDoNotFitSystemUi) {
  EXPECT_TRUE(uiFontSelection::hasSupportedMetrics(23, 23, 21, -2));
  EXPECT_TRUE(uiFontSelection::hasSupportedMetrics(16, 16, 14, -2));
  EXPECT_FALSE(uiFontSelection::hasSupportedMetrics(5, 5, 4, -1));
  EXPECT_FALSE(uiFontSelection::hasSupportedMetrics(48, 48, 44, -4));
  EXPECT_FALSE(uiFontSelection::hasSupportedMetrics(18, 18, 40, -2));
  EXPECT_FALSE(uiFontSelection::hasSupportedMetrics(18, 18, 16, -24));
}

TEST(UiFontSelection, MigratesOnlyLegacySdUiModeWithAReaderSelection) {
  EXPECT_FALSE(uiFontSelection::shouldDeferLegacyMigration(true, true, "ReaderA"));
  EXPECT_FALSE(uiFontSelection::shouldDeferLegacyMigration(false, false, "ReaderA"));
  EXPECT_FALSE(uiFontSelection::shouldDeferLegacyMigration(false, true, ""));
  EXPECT_TRUE(uiFontSelection::shouldDeferLegacyMigration(false, true, "ReaderA"));
}

TEST(UiFontSelection, KeepsFileChoicesBeyondByteIndexRange) {
  std::vector<std::string> options = {"Built-in"};
  options.reserve(301);
  for (int i = 0; i < 300; ++i) options.push_back("/.fonts/Family/Face_" + std::to_string(i) + ".cpfont");

  EXPECT_EQ(uiFontSelection::indexForPath(options, 300, 0, options[280].c_str()), 280);
  EXPECT_EQ(uiFontSelection::indexForPath(options, 300, 301, "/.fonts/Family/missing.cpfont"), 301);
}

TEST(UiFontSelection, ChangesUiPathWithoutChangingReaderPath) {
  std::vector<std::string> options = {"Built-in", "/.fonts/Family/A_14.cpfont", "/.fonts/Family/B_23.cpfont",
                                      "Unavailable: /.fonts/Family/removed.cpfont"};
  char readerFontPath[64] = "/fonts/Reader/Reader_16.cpfont";
  char uiFontPath[64] = "/.fonts/Family/A_14.cpfont";

  ASSERT_TRUE(uiFontSelection::setPathForIndex(uiFontPath, sizeof(uiFontPath), options, 2, 3, 2));
  EXPECT_STREQ(uiFontPath, "/.fonts/Family/B_23.cpfont");
  EXPECT_STREQ(readerFontPath, "/fonts/Reader/Reader_16.cpfont");

  ASSERT_TRUE(uiFontSelection::setPathForIndex(uiFontPath, sizeof(uiFontPath), options, 2, 3, 3));
  EXPECT_STREQ(uiFontPath, "/.fonts/Family/B_23.cpfont");

  ASSERT_TRUE(uiFontSelection::setPathForIndex(uiFontPath, sizeof(uiFontPath), options, 2, 3, 0));
  EXPECT_STREQ(uiFontPath, "");
  EXPECT_STREQ(readerFontPath, "/fonts/Reader/Reader_16.cpfont");
}

TEST(UiFontSelection, ReaderAndUiChoicesRemainIndependentAcrossCombinations) {
  const std::vector<std::string> options = {"Built-in", "/.fonts/Family/A_14.cpfont", "/.fonts/Family/B_23.cpfont"};
  struct Combination {
    const char* readerPath;
    const char* uiPath;
  };
  constexpr Combination COMBINATIONS[] = {
      {"", ""},
      {"/fonts/Reader/A_14.cpfont", ""},
      {"", "/.fonts/Family/A_14.cpfont"},
      {"/fonts/Reader/A_14.cpfont", "/.fonts/Family/B_23.cpfont"},
      {"/fonts/Reader/A_14.cpfont", "/.fonts/Family/A_14.cpfont"},
  };

  for (const auto& combination : COMBINATIONS) {
    char readerPath[64];
    char uiPath[64];
    std::snprintf(readerPath, sizeof(readerPath), "%s", combination.readerPath);
    std::snprintf(uiPath, sizeof(uiPath), "%s", combination.uiPath);
    const std::string savedReaderPath = readerPath;
    const uint16_t selection = uiFontSelection::indexForPath(options, 2, 0, uiPath);

    ASSERT_TRUE(uiFontSelection::setPathForIndex(uiPath, sizeof(uiPath), options, 2, 0, selection));
    EXPECT_STREQ(readerPath, savedReaderPath.c_str());
    EXPECT_STREQ(uiPath, combination.uiPath);
    EXPECT_EQ(uiFontSelection::indexForPath(options, 2, 0, uiPath), selection);
  }
}

TEST(UiFontSelection, MissingUiFileKeepsItsPathForWhenSdReturns) {
  const std::vector<std::string> missingOptions = {"Built-in", "/.fonts/Family/B_23.cpfont",
                                                   "Unavailable: /.fonts/Family/A_14.cpfont"};
  char uiPath[64] = "/.fonts/Family/A_14.cpfont";
  EXPECT_EQ(uiFontSelection::indexForPath(missingOptions, 1, 2, uiPath), 2);
  ASSERT_TRUE(uiFontSelection::setPathForIndex(uiPath, sizeof(uiPath), missingOptions, 1, 2, 2));
  EXPECT_STREQ(uiPath, "/.fonts/Family/A_14.cpfont");

  const std::vector<std::string> restoredOptions = {"Built-in", "/.fonts/Family/A_14.cpfont",
                                                    "/.fonts/Family/B_23.cpfont"};
  EXPECT_EQ(uiFontSelection::indexForPath(restoredOptions, 2, 0, uiPath), 1);
}

TEST(OptionPopupPaging, SelectionPagesReachOptionsPastTheFirstPage) {
  constexpr size_t TOTAL_OPTIONS = 300;
  const auto first = optionPopupPaging::windowForSelection(TOTAL_OPTIONS, 0);
  const auto middle = optionPopupPaging::windowForSelection(TOTAL_OPTIONS, 279);
  const auto last = optionPopupPaging::windowForSelection(TOTAL_OPTIONS, TOTAL_OPTIONS - 1);

  EXPECT_EQ(first.first, 0);
  EXPECT_EQ(first.count, 16);
  EXPECT_EQ(middle.first, 272);
  EXPECT_EQ(middle.count, 16);
  EXPECT_EQ(last.first, 288);
  EXPECT_EQ(last.count, 12);
  EXPECT_EQ(optionPopupPaging::adjacentSelection(TOTAL_OPTIONS, 279, false), 280);
  EXPECT_EQ(optionPopupPaging::adjacentSelection(TOTAL_OPTIONS, 279, true), 278);
  EXPECT_EQ(optionPopupPaging::adjacentSelection(TOTAL_OPTIONS, TOTAL_OPTIONS - 1, false), 0);
}

TEST(OptionPopupPaging, FitsRowsToAvailableHeightAndFontMetrics) {
  constexpr size_t TOTAL_OPTIONS = 100;
  constexpr size_t MAX_OPTIONS = 16;
  constexpr size_t AVAILABLE_HEIGHT = 414;
  constexpr size_t BUTTON_HEIGHT = 48;
  constexpr size_t GAP = 8;

  const size_t visible =
      optionPopupPaging::visibleCountForHeight(TOTAL_OPTIONS, MAX_OPTIONS, AVAILABLE_HEIGHT, BUTTON_HEIGHT, GAP);
  EXPECT_EQ(visible, 7);
  EXPECT_LE(GAP + visible * BUTTON_HEIGHT + (visible - 1) * GAP, AVAILABLE_HEIGHT);
  EXPECT_GT(GAP + (visible + 1) * BUTTON_HEIGHT + visible * GAP, AVAILABLE_HEIGHT);

  const auto window = optionPopupPaging::windowForSelection(TOTAL_OPTIONS, 45, visible);
  EXPECT_EQ(window.first, 42);
  EXPECT_EQ(window.count, 7);
  EXPECT_EQ(optionPopupPaging::visibleCountForHeight(TOTAL_OPTIONS, MAX_OPTIONS, 0, BUTTON_HEIGHT, GAP), 0);
  EXPECT_EQ(optionPopupPaging::visibleCountForHeight(TOTAL_OPTIONS, MAX_OPTIONS, 600, 23, GAP), MAX_OPTIONS);
}
