#include <Arduino.h>
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

namespace {

class DictionaryHtmlPagesTest : public ::testing::Test {
 protected:
  GfxRenderer renderer;
  std::vector<std::unique_ptr<Page>> pages;

  void SetUp() override {
    ESP = {};
    renderer.sdFont = true;
#ifdef _WIN32
    const auto pid = _getpid();
#else
    const auto pid = getpid();
#endif
    Storage.dictionaryTempPath =
        (std::filesystem::temp_directory_path() / ("crosspoint-dicthtml-" + std::to_string(pid) + "-" +
                                                   testing::UnitTest::GetInstance()->current_test_info()->name()))
            .string();
  }
  void TearDown() override {
    std::filesystem::remove(Storage.dictionaryTempPath);
    Storage.dictionaryTempPath.clear();
    ESP = {};
  }

  void expectEarlyStop(uint32_t freeHeap, uint32_t maxAllocHeap) {
    std::string definition;
    for (int i = 0; i < 100; ++i) definition += "<p>one two three four five six seven eight</p>";
    renderer.onFontPrepare = [=] {
      ESP.freeHeap = freeHeap;
      ESP.maxAllocHeap = maxAllocHeap;
    };
    EXPECT_FALSE(buildDictionaryHtmlPages(renderer, definition, 160, 16, pages));
    EXPECT_TRUE(pages.empty());
    EXPECT_EQ(renderer.fontPreparations, 1u);
    EXPECT_FALSE(Storage.exists(Storage.dictionaryTempPath.c_str()));
  }
};

TEST_F(DictionaryHtmlPagesTest, SmallDefinitionKeepsStyledPages) {
  ASSERT_TRUE(buildDictionaryHtmlPages(renderer, "<p>one <b>two</b> three</p>", 160, 64, pages));
  ASSERT_EQ(pages.size(), 1u);
  EXPECT_FALSE(pages.front()->elements.empty());
  EXPECT_FALSE(Storage.exists(Storage.dictionaryTempPath.c_str()));
}

}  // namespace
