#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstring>

#include "src/util/MinimalHomeSettingsJson.h"

TEST(MinimalHomeSettingsJson, PreservesLegacyDefaultsWhenKeysAreAbsent) {
  JsonDocument legacy;
  std::array<char, 256> path{};
  uint8_t alignment = minimal_home_settings::kDefaultAlignment;

  EXPECT_FALSE(minimal_home_settings::readJson(legacy.as<JsonVariantConst>(), alignment, path.data(), path.size()));
  EXPECT_EQ(alignment, minimal_home_settings::kDefaultAlignment);
  EXPECT_STREQ(path.data(), "");
}

TEST(MinimalHomeSettingsJson, RoundTripsAlignmentAndBackgroundPath) {
  JsonDocument written;
  std::array<char, 256> original{};
  std::array<char, 256> restored{};
  std::snprintf(original.data(), original.size(), "/wallpapers/home.bmp");

  minimal_home_settings::writeJson(written, 2, original.data());
  EXPECT_EQ(written[minimal_home_settings::kAlignmentKey].as<uint8_t>(), 2);
  EXPECT_STREQ(written[minimal_home_settings::kBackgroundPathKey].as<const char*>(), original.data());

  uint8_t alignment = minimal_home_settings::kDefaultAlignment;
  EXPECT_FALSE(minimal_home_settings::readJson(written.as<JsonVariantConst>(), alignment, restored.data(), restored.size()));
  EXPECT_EQ(alignment, 2);
  EXPECT_STREQ(restored.data(), original.data());
}

TEST(MinimalHomeSettingsJson, ClampsInvalidAlignmentAndClearsOversizedPath) {
  JsonDocument invalid;
  invalid[minimal_home_settings::kAlignmentKey] = 255;
  std::array<char, 8> path{};
  invalid[minimal_home_settings::kBackgroundPathKey] = "/too-long.bmp";
  uint8_t alignment = 1;

  EXPECT_TRUE(minimal_home_settings::readJson(invalid.as<JsonVariantConst>(), alignment, path.data(), path.size()));
  EXPECT_EQ(alignment, minimal_home_settings::kDefaultAlignment);
  EXPECT_STREQ(path.data(), "");
}
