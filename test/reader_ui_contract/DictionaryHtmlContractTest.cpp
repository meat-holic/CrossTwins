#include <Arduino.h>
#include <Epub/blocks/TextBlock.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <filesystem>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "src/util/DictHtmlPages.h"

class DictionaryHtmlContract : public testing::Test {
 protected:
  GfxRenderer renderer;
  std::vector<std::unique_ptr<Page>> pages;
  void SetUp() override {
    ESP = {};
    HalFileTest::failNextWrite = HalFileTest::failNextClose = false;
#ifdef _WIN32
    const auto pid = _getpid();
#else
    const auto pid = getpid();
#endif
    Storage.dictionaryTempPath =
        (std::filesystem::temp_directory_path() / ("ct8-dictionary-" + std::to_string(pid) + "-" +
                                                   testing::UnitTest::GetInstance()->current_test_info()->name()))
            .string();
  }
  void TearDown() override {
    EXPECT_FALSE(std::filesystem::exists(Storage.dictionaryTempPath));
    std::filesystem::remove(Storage.dictionaryTempPath);
    Storage.dictionaryTempPath.clear();
    ESP = {};
  }
};

TEST_F(DictionaryHtmlContract, StyledKoreanAndEntitiesSurviveNormalization) {
  ASSERT_TRUE(buildDictionaryHtmlPages(renderer, "<p>한글 <b>뜻</b> &amp; EN<br>line</p>", 200, 200, pages));
  ASSERT_FALSE(pages.empty());
  std::string text;
  for (const auto& page : pages) {
    for (const auto& element : page->elements) {
      if (element->getTag() != TAG_PageLine) continue;
      const auto& block = static_cast<const PageLine&>(*element).getBlock();
      for (uint16_t i = 0; i < block->wordCount(); ++i) text += block->wordText(i);
    }
  }
  EXPECT_NE(text.find("한글"), std::string::npos);
  EXPECT_NE(text.find("뜻"), std::string::npos);
  EXPECT_NE(text.find('&'), std::string::npos);
}

TEST_F(DictionaryHtmlContract, WriteFailureAndReentryReleaseTemporaryFileAndPages) {
  HalFileTest::failNextWrite = true;
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, "<p>definition</p>", 200, 200, pages));
  EXPECT_TRUE(pages.empty());
  EXPECT_FALSE(std::filesystem::exists(Storage.dictionaryTempPath));
  EXPECT_TRUE(buildDictionaryHtmlPages(renderer, "<p>retry</p>", 200, 200, pages));
}

TEST_F(DictionaryHtmlContract, CloseFailureCannotPublishStyledPages) {
  HalFileTest::failNextClose = true;
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, "<p>definition</p>", 200, 200, pages));
  EXPECT_TRUE(pages.empty());
}

TEST_F(DictionaryHtmlContract, LowHeapFallbackDiscardsPreviousOutput) {
  ASSERT_TRUE(buildDictionaryHtmlPages(renderer, "<p>old</p>", 200, 200, pages));
  ESP.freeHeap = 39 * 1024;
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, "<p>new</p>", 200, 200, pages));
  EXPECT_TRUE(pages.empty());
}

TEST_F(DictionaryHtmlContract, RetainLimitReturnsNoPartialDefinition) {
  renderer.onMeasure = [] { ESP.freeHeap = 15 * 1024; };
  std::string html;
  for (int i = 0; i < 30; ++i) html += "<p>one two three four</p>";
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, html, 160, 32, pages));
  EXPECT_TRUE(pages.empty());
}

TEST_F(DictionaryHtmlContract, PageCountCapDiscardsOutputAndTemporaryFileAtNormalHeap) {
  std::string definition;
  for (int i = 0; i < 100; ++i) definition += "<p>one two three four five six seven eight</p>";
  const auto initialHeap = ESP.freeHeap;
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, definition, 160, 16, pages));
  EXPECT_TRUE(pages.empty());
  EXPECT_EQ(ESP.freeHeap, initialHeap);
  EXPECT_FALSE(std::filesystem::exists(Storage.dictionaryTempPath));
}

TEST_F(DictionaryHtmlContract, PageCapBoundaryRetains64ButRejects65AtNormalHeap) {
  for (int count : {63, 64, 65}) {
    std::string definition;
    for (int i = 0; i < count; ++i) definition += "<p>one</p>";
    const bool ok = buildDictionaryHtmlPages(renderer, definition, 160, 16, pages);
    EXPECT_EQ(ok, count <= 64) << count;
    EXPECT_EQ(pages.size(), count <= 64 ? static_cast<size_t>(count) : 0u) << count;
    EXPECT_FALSE(std::filesystem::exists(Storage.dictionaryTempPath));
  }
}
