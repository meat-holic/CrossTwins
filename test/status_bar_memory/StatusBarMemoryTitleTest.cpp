#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

#include "StatusBarMemoryTitle.h"

namespace {
constexpr char kFormat[] = "HEAP %lluK / BLOCK %lluK";
constexpr char kUnavailable[] = "HEAP N/A / BLOCK N/A";

TEST(StatusBarMemoryTitle, FloorsBothMeasurementsToWholeKibibytes) {
  std::array<char, status_bar_memory::kTitleBufferSize> title{};
  constexpr size_t kib = 1024;

  ASSERT_TRUE(status_bar_memory::formatTitle(title.data(), title.size(), 142 * kib + 1023, 87 * kib + 1023, kFormat,
                                             kUnavailable));
  EXPECT_STREQ("HEAP 142K / BLOCK 87K", title.data());
}

TEST(StatusBarMemoryTitle, DisplaysZeroMeasurementsAsZero) {
  std::array<char, status_bar_memory::kTitleBufferSize> title{};

  ASSERT_TRUE(status_bar_memory::formatTitle(title.data(), title.size(), 0, 0, kFormat, kUnavailable));
  EXPECT_STREQ("HEAP 0K / BLOCK 0K", title.data());
}

TEST(StatusBarMemoryTitle, ReportsAnImpossibleLargestBlockAsUnavailable) {
  std::array<char, status_bar_memory::kTitleBufferSize> title{};

  ASSERT_TRUE(status_bar_memory::formatTitle(title.data(), title.size(), 100, 101, kFormat, kUnavailable));
  EXPECT_STREQ(kUnavailable, title.data());
}

TEST(StatusBarMemoryTitle, HandlesMaximumSizeValuesWithinTheFixedBuffer) {
  std::array<char, status_bar_memory::kTitleBufferSize> title{};
  const auto max = std::numeric_limits<size_t>::max();

  ASSERT_TRUE(status_bar_memory::formatTitle(title.data(), title.size(), max, max, kFormat, kUnavailable));
  EXPECT_LT(std::char_traits<char>::length(title.data()), title.size());
}

TEST(StatusBarMemoryTitle, LeavesShortOutputNullTerminated) {
  std::array<char, 8> title{};

  EXPECT_FALSE(
      status_bar_memory::formatTitle(title.data(), title.size(), 142 * 1024, 87 * 1024, kFormat, kUnavailable));
  EXPECT_EQ('\0', title.back());
}
}  // namespace
