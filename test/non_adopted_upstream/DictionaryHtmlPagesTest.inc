// UPSTREAM_ONLY: Upstream paragraph-stop API and 60KiB entry gate are not CT per-page retain gates (40/20KiB entry,16/8KiB retain).

TEST_F(DictionaryHtmlPagesTest, LowHeadroomStopsBeforePreparingAnotherParagraph) {
  expectEarlyStop(30 * 1024, UINT32_MAX);
}

TEST_F(DictionaryHtmlPagesTest, FragmentationStopsBeforePreparingAnotherParagraph) {
  expectEarlyStop(UINT32_MAX, 24 * 1024);
}

TEST_F(DictionaryHtmlPagesTest, LowEntryHeapAvoidsStartingParser) {
  ESP.freeHeap = 60 * 1024;
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, "<p>one two three</p>", 160, 16, pages));
  EXPECT_TRUE(pages.empty());
  EXPECT_EQ(renderer.fontPreparations, 0u);
}

TEST_F(DictionaryHtmlPagesTest, PageCountLimitAlsoStopsParsing) {
  std::string definition;
  for (int i = 0; i < 100; ++i) definition += "<p>one two three four five six seven eight</p>";
  EXPECT_FALSE(buildDictionaryHtmlPages(renderer, definition, 160, 16, pages));
  EXPECT_TRUE(pages.empty());
  EXPECT_LT(renderer.fontPreparations, 100u);
  EXPECT_FALSE(Storage.exists(Storage.dictionaryTempPath.c_str()));
}
