#include <gtest/gtest.h>

#include <cstring>

#include "util/UiFontSettingsJson.h"

namespace {
constexpr size_t FAMILY_CAPACITY = 128;
constexpr size_t PATH_CAPACITY = 160;
constexpr char LEGACY_PATH[] = "/.fonts/Sam3KRFont/Sam3KRFont_12.cpfont";

const char* installedFamily(const char* path) { return strcmp(path, LEGACY_PATH) == 0 ? "Sam3KRFont" : nullptr; }
}  // namespace

TEST(UiFontSettings, MigratesExactInstalledFileOnceAndKeepsOtherValues) {
  JsonDocument doc;
  doc["uiFontMode"] = 1;
  doc["uiSdFontPath"] = LEGACY_PATH;
  doc["language"] = "KO";
  doc["lineHeight"] = 4;
  EXPECT_STREQ(ui_font_settings::migrate(doc, FAMILY_CAPACITY, PATH_CAPACITY, installedFamily), "Sam3KRFont");
  EXPECT_STREQ(doc["uiFontFamilyName"].as<const char*>(), "Sam3KRFont");
  EXPECT_STREQ(doc["language"].as<const char*>(), "KO");
  EXPECT_EQ(doc["lineHeight"].as<int>(), 4);
  EXPECT_EQ(ui_font_settings::migrate(doc, FAMILY_CAPACITY, PATH_CAPACITY, installedFamily), nullptr);
}

TEST(UiFontSettings, PreservesNewSelectionIncludingExplicitBuiltIn) {
  for (const char* family : {"NotoSerif", ""}) {
    JsonDocument doc;
    doc["uiFontFamilyName"] = family;
    doc["uiSdFontPath"] = LEGACY_PATH;
    EXPECT_EQ(ui_font_settings::migrate(doc, FAMILY_CAPACITY, PATH_CAPACITY, installedFamily), nullptr);
    EXPECT_STREQ(doc["uiFontFamilyName"].as<const char*>(), family);
  }
}

TEST(UiFontSettings, DoesNotMigrateMissingFileOrDisabledLegacyChoice) {
  JsonDocument doc;
  doc["uiSdFontPath"] = "/fonts/Absent/Absent_10.cpfont";
  EXPECT_EQ(ui_font_settings::migrate(doc, FAMILY_CAPACITY, PATH_CAPACITY, installedFamily), nullptr);
  EXPECT_TRUE(doc["uiFontFamilyName"].isUnbound());
  doc["uiSdFontPath"] = LEGACY_PATH;
  doc["uiFontMode"] = 0;
  EXPECT_EQ(ui_font_settings::migrate(doc, FAMILY_CAPACITY, PATH_CAPACITY, installedFamily), nullptr);
  EXPECT_TRUE(doc["uiFontFamilyName"].isUnbound());
}

TEST(UiFontSettings, UsesLegacyPathWhenNewValueIsMalformed) {
  JsonDocument doc;
  doc["uiFontFamilyName"] = 10;
  doc["uiSdFontPath"] = LEGACY_PATH;
  EXPECT_STREQ(ui_font_settings::migrate(doc, FAMILY_CAPACITY, PATH_CAPACITY, installedFamily), "Sam3KRFont");
}
