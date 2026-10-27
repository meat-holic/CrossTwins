#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "ReleaseAssetName.h"
#include "ReleaseVersion.h"

TEST(ReleaseVersion, OrdersRcRevisionsBeforeTheFinalRelease) {
  EXPECT_TRUE(release_version::isNewer("2.0.0-rc2.rev.1", "2.0.0-rc2"));
  EXPECT_TRUE(release_version::isNewer("2.0.0-rc2.rev.2", "2.0.0-rc2.rev.1"));
  EXPECT_TRUE(release_version::isNewer("2.0.0", "2.0.0-rc2.rev.2"));
}

TEST(ReleaseVersion, HandlesVPrefixAndRejectsEqualOrOlderVersions) {
  EXPECT_TRUE(release_version::isNewer("v2.0.0-rc2.rev.2", "2.0.0-rc2.rev.1"));
  EXPECT_FALSE(release_version::isNewer("v2.0.0-rc2.rev.2", "2.0.0-rc2.rev.2"));
  EXPECT_FALSE(release_version::isNewer("2.0.0-rc2.rev.1", "2.0.0-rc2.rev.2"));
  EXPECT_FALSE(release_version::isNewer("2.0.0-rc1.rev.9", "2.0.0-rc2"));
}

TEST(ReleaseVersion, ComparesMajorMinorPatchAndRcNumbers) {
  EXPECT_TRUE(release_version::isNewer("3.0.0-rc1", "2.99.99"));
  EXPECT_TRUE(release_version::isNewer("2.1.0-rc1", "2.0.99"));
  EXPECT_TRUE(release_version::isNewer("2.0.1-rc1", "2.0.0"));
  EXPECT_TRUE(release_version::isNewer("2.0.0-rc3", "2.0.0-rc2.rev.99"));
  EXPECT_FALSE(release_version::isNewer("2.0.0-rc2.rev.99", "2.0.0"));
}

TEST(ReleaseVersion, RejectsMalformedTagsAndOverflow) {
  constexpr std::array<std::string_view, 12> invalid = {
      "",          "v",             "2.0",          "2.0.0.1",     "vv2.0.0", "2.0.0-rc",
      "2.0.0-RC2", "2.0.0-rc2.foo", "2.0.0-rc2.rev", "02.0.0",      "2.0.0-rc02", "4294967296.0.0"};
  for (const auto version : invalid) {
    release_version::Version parsed;
    EXPECT_FALSE(release_version::parse(version, parsed)) << version;
    EXPECT_FALSE(release_version::isNewer(version, "2.0.0")) << version;
  }
  EXPECT_FALSE(release_version::isNewer("2.0.0", "2.0.0-build.1"));
}

TEST(ReleaseAssetName, MapsOnlySupportedBoardsToOfficialAssets) {
  constexpr std::array<std::pair<std::string_view, std::string_view>, 6> mappings = {{
      {"x3", "x3-x4"}, {"x4", "x3-x4"}, {"sticky", "sticky"},
      {"x4pro", "x4pro"}, {"x4c", "x4c"}, {"papermono", "papermono"},
  }};
  for (const auto& [board, device] : mappings) EXPECT_EQ(release_asset::deviceForBoard(board), device);
  EXPECT_TRUE(release_asset::deviceForBoard("m5paper").empty());
  EXPECT_TRUE(release_asset::deviceForBoard("metalio_eink4").empty());
}

TEST(ReleaseAssetName, NormalizesTagWithoutChangingItsStoredSpelling) {
  char name[80] = {};
  constexpr std::string_view originalTag = "v2.0.0-rc2.rev.2";
  ASSERT_TRUE(release_asset::buildFirmwareAssetName(originalTag, "x4", name, sizeof(name)));
  EXPECT_EQ(originalTag, "v2.0.0-rc2.rev.2");
  EXPECT_STREQ(name, "crosstwins-2.0.0-rc2.rev.2-x3-x4.bin");
  EXPECT_TRUE(release_asset::buildFirmwareAssetName("2.0.0-rc2.rev.2", "papermono", name, sizeof(name)));
  EXPECT_STREQ(name, "crosstwins-2.0.0-rc2.rev.2-papermono.bin");
}

TEST(ReleaseAssetName, RejectsMalformedTagsUnknownBoardsAndSmallBuffers) {
  char name[16] = {};
  EXPECT_FALSE(release_asset::buildFirmwareAssetName("v2.0.0-rc2.rev.2", "m5paper", name, sizeof(name)));
  EXPECT_FALSE(release_asset::buildFirmwareAssetName("2.0.0-rc2.rev.2-dev", "x4", name, sizeof(name)));
  EXPECT_FALSE(release_asset::buildFirmwareAssetName("2.0.0-rc2.rev.2", "x4", name, sizeof(name)));
}
