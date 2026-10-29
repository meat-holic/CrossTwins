#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "util/ReaderLineSpacingJson.h"

namespace {
constexpr uint8_t OPTION_COUNT = 7;
constexpr uint8_t DEFAULT_VALUE = 2;
}  // namespace

TEST(CrossPointSettingsLineSpacingJson, CurrentKeySurvivesJsonRoundTrip) {
  constexpr std::array<uint8_t, OPTION_COUNT> VALUES = {0, 1, 2, 3, 4, 5, 6};

  for (const uint8_t expected : VALUES) {
    JsonDocument written;
    reader_line_spacing::writeJson(written, expected);
    ASSERT_TRUE(written["lineHeight"].is<uint8_t>());
    EXPECT_TRUE(written["lineSpacing"].isNull());

    char json[64] = {};
    const size_t length = serializeJson(written, json, sizeof(json));
    ASSERT_GT(length, 0u);

    JsonDocument parsed;
    ASSERT_FALSE(deserializeJson(parsed, json));
    uint8_t restored = DEFAULT_VALUE;
    EXPECT_FALSE(reader_line_spacing::readJson(parsed.as<JsonVariantConst>(), restored, OPTION_COUNT, DEFAULT_VALUE));
    EXPECT_EQ(restored, expected);
  }
}

TEST(CrossPointSettingsLineSpacingJson, CurrentKeyTakesPriorityOverLegacyKey) {
  JsonDocument document;
  ASSERT_FALSE(deserializeJson(document, R"({"lineHeight":4,"lineSpacing":1})"));

  uint8_t value = DEFAULT_VALUE;
  EXPECT_FALSE(reader_line_spacing::readJson(document.as<JsonVariantConst>(), value, OPTION_COUNT, DEFAULT_VALUE));
  EXPECT_EQ(value, 4);
}

TEST(CrossPointSettingsLineSpacingJson, LegacyKeyMigratesOnlyWhenCurrentKeyIsAbsent) {
  JsonDocument document;
  ASSERT_FALSE(deserializeJson(document, R"({"lineSpacing":1})"));

  uint8_t value = DEFAULT_VALUE;
  EXPECT_TRUE(reader_line_spacing::readJson(document.as<JsonVariantConst>(), value, OPTION_COUNT, DEFAULT_VALUE));
  EXPECT_EQ(value, 2);
}

TEST(CrossPointSettingsLineSpacingJson, InvalidCurrentValueUsesDefaultAndIgnoresLegacyValue) {
  JsonDocument document;
  ASSERT_FALSE(deserializeJson(document, R"({"lineHeight":255,"lineSpacing":1})"));

  uint8_t value = 4;
  EXPECT_FALSE(reader_line_spacing::readJson(document.as<JsonVariantConst>(), value, OPTION_COUNT, DEFAULT_VALUE));
  EXPECT_EQ(value, DEFAULT_VALUE);
}

TEST(CrossPointSettingsLineSpacingJson, PresentButMalformedCurrentKeyDoesNotFallBackToLegacy) {
  for (const char* json : {R"({"lineHeight":null,"lineSpacing":1})", R"({"lineHeight":-1,"lineSpacing":1})",
                           R"({"lineHeight":1.5,"lineSpacing":1})", R"({"lineHeight":true,"lineSpacing":1})"}) {
    JsonDocument document;
    ASSERT_FALSE(deserializeJson(document, json));

    uint8_t value = 4;
    EXPECT_FALSE(reader_line_spacing::readJson(document.as<JsonVariantConst>(), value, OPTION_COUNT, DEFAULT_VALUE));
    EXPECT_EQ(value, DEFAULT_VALUE);
  }
}

TEST(CrossPointSettingsLineSpacingJson, WebRequestMigratesLegacyKeyAndPrefersCurrentKey) {
  JsonDocument legacyDocument;
  ASSERT_FALSE(deserializeJson(legacyDocument, R"({"lineSpacing":1})"));
  uint8_t value = DEFAULT_VALUE;
  EXPECT_EQ(reader_line_spacing::readWebRequest(legacyDocument.as<JsonVariantConst>(), value, OPTION_COUNT),
            reader_line_spacing::WebRequestState::VALID);
  EXPECT_EQ(value, 2);

  JsonDocument currentDocument;
  ASSERT_FALSE(deserializeJson(currentDocument, R"({"lineHeight":4,"lineSpacing":1})"));
  EXPECT_EQ(reader_line_spacing::readWebRequest(currentDocument.as<JsonVariantConst>(), value, OPTION_COUNT),
            reader_line_spacing::WebRequestState::VALID);
  EXPECT_EQ(value, 4);
}

TEST(CrossPointSettingsLineSpacingJson, WebRequestRejectsMalformedCurrentKeyWithoutLegacyFallback) {
  for (const char* json : {R"({"lineHeight":null,"lineSpacing":1})", R"({"lineHeight":"invalid","lineSpacing":1})",
                           R"({"lineHeight":-1,"lineSpacing":1})", R"({"lineHeight":1.5,"lineSpacing":1})",
                           R"({"lineHeight":true,"lineSpacing":1})"}) {
    JsonDocument document;
    ASSERT_FALSE(deserializeJson(document, json));

    uint8_t value = DEFAULT_VALUE;
    EXPECT_EQ(reader_line_spacing::readWebRequest(document.as<JsonVariantConst>(), value, OPTION_COUNT),
              reader_line_spacing::WebRequestState::INVALID);
    EXPECT_EQ(value, DEFAULT_VALUE);
  }
}
