#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include "util/PluginEventMetadata.h"

TEST(PluginEventMeta, MissingFilterMeansUnfiltered) {
  JsonDocument document;
  ASSERT_FALSE(deserializeJson(document, R"({"request":{}})"));

  const char* requiredMeta = nullptr;
  EXPECT_TRUE(pluginevents::detail::readRequiredMeta(document["request"], requiredMeta));
  ASSERT_NE(requiredMeta, nullptr);
  EXPECT_STREQ(requiredMeta, "");
}

TEST(PluginEventMeta, StringFilterIsAccepted) {
  JsonDocument document;
  ASSERT_FALSE(deserializeJson(document, R"({"require_meta":"title"})"));

  const char* requiredMeta = nullptr;
  EXPECT_TRUE(pluginevents::detail::readRequiredMeta(document.as<JsonVariantConst>(), requiredMeta));
  ASSERT_NE(requiredMeta, nullptr);
  EXPECT_STREQ(requiredMeta, "title");
}

TEST(PluginEventMeta, ExplicitNullDoesNotDisableFilterValidation) {
  JsonDocument document;
  ASSERT_FALSE(deserializeJson(document, R"({"require_meta":null})"));

  const char* requiredMeta = nullptr;
  EXPECT_FALSE(pluginevents::detail::readRequiredMeta(document.as<JsonVariantConst>(), requiredMeta));
}

TEST(PluginEventMeta, OtherNonStringFiltersAreRejected) {
  for (const char* json : {R"({"require_meta":1})", R"({"require_meta":false})", R"({"require_meta":[]})",
                           R"({"require_meta":{}})"}) {
    JsonDocument document;
    ASSERT_FALSE(deserializeJson(document, json));

    const char* requiredMeta = nullptr;
    EXPECT_FALSE(pluginevents::detail::readRequiredMeta(document.as<JsonVariantConst>(), requiredMeta)) << json;
  }
}
