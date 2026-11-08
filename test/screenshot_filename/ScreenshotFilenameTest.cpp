#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

#include "src/util/ScreenshotFilename.h"

TEST(ScreenshotFilename, EmptyDirectoryStartsAtOne) {
  uint64_t next = 0;
  EXPECT_TRUE(screenshot_filename::nextAfter(0, next));
  EXPECT_EQ(next, 1u);

  char path[64];
  ASSERT_TRUE(screenshot_filename::formatPath("/screenshots", next, path, sizeof(path)));
  EXPECT_STREQ(path, "/screenshots/screenshot_00001.bmp");
}

TEST(ScreenshotFilename, ExistingMaximumWinsAcrossGaps) {
  const std::array<const char*, 5> names = {"screenshot_00001.bmp", "screenshot_00003.bmp",
                                             "screenshot_00008.bmp", "screenshot-123456.bmp",
                                             "book_p4_12pct_99.bmp"};
  uint64_t largest = 0;
  for (const char* name : names) ASSERT_TRUE(screenshot_filename::observeName(name, largest));

  uint64_t next = 0;
  ASSERT_TRUE(screenshot_filename::nextAfter(largest, next));
  EXPECT_EQ(next, 9u);
}

TEST(ScreenshotFilename, LegacyTimestampNamesAreIgnored) {
  uint64_t largest = 4;
  EXPECT_TRUE(screenshot_filename::observeName("screenshot-123456.bmp", largest));
  EXPECT_TRUE(screenshot_filename::observeName("title_p7_42pct_123456.bmp", largest));
  EXPECT_EQ(largest, 4u);
}

TEST(ScreenshotFilename, MatchingRequiresAtLeastFiveDigitsAndLowercaseBmpSuffix) {
  uint64_t number = 0;
  EXPECT_EQ(screenshot_filename::parseNumber("screenshot_00001.bmp", number),
            screenshot_filename::ParseResult::Valid);
  EXPECT_EQ(number, 1u);
  EXPECT_EQ(screenshot_filename::parseNumber("screenshot_1234.bmp", number),
            screenshot_filename::ParseResult::NotNumberedScreenshot);
  EXPECT_EQ(screenshot_filename::parseNumber("screenshot_12345.BMP", number),
            screenshot_filename::ParseResult::NotNumberedScreenshot);
  EXPECT_EQ(screenshot_filename::parseNumber("screenshot_12x45.bmp", number),
            screenshot_filename::ParseResult::NotNumberedScreenshot);
}

TEST(ScreenshotFilename, WidthExpandsPastFiveDigits) {
  char path[64];
  ASSERT_TRUE(screenshot_filename::formatPath("/screenshots", 99999, path, sizeof(path)));
  EXPECT_STREQ(path, "/screenshots/screenshot_99999.bmp");
  ASSERT_TRUE(screenshot_filename::formatPath("/screenshots", 100000, path, sizeof(path)));
  EXPECT_STREQ(path, "/screenshots/screenshot_100000.bmp");
}

TEST(ScreenshotFilename, CollisionAdvancesWithoutWrapping) {
  uint64_t next = 17;
  ASSERT_TRUE(screenshot_filename::increment(next));
  EXPECT_EQ(next, 18u);

  next = std::numeric_limits<uint64_t>::max();
  EXPECT_FALSE(screenshot_filename::increment(next));
  EXPECT_EQ(next, std::numeric_limits<uint64_t>::max());
}

TEST(ScreenshotFilename, NumericOverflowFailsTheDirectoryScan) {
  uint64_t largest = 7;
  EXPECT_FALSE(screenshot_filename::observeName("screenshot_18446744073709551616.bmp", largest));
  EXPECT_EQ(largest, 7u);
  EXPECT_FALSE(screenshot_filename::nextAfter(std::numeric_limits<uint64_t>::max(), largest));
}

TEST(ScreenshotFilename, PathBufferOverflowIsReported) {
  char path[12] = {};
  EXPECT_FALSE(screenshot_filename::formatPath("/screenshots", 1, path, sizeof(path)));
}
