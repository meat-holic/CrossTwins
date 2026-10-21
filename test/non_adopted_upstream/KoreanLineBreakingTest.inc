// UPSTREAM_ONLY — not registered in CT CMake.
// Clipping SourceRange payload is excluded; CT typed renderer and Korean golden expectations remain unchanged.

TEST(ClippingAnchors, SourceCoverageSurvivesWrappingAndHyphenation) {
  Hyphenator::setPreferredLanguage("ko");
  GfxRenderer renderer;
  for (const int width : {50, 180}) {
    for (const bool hyphenation : {false, true}) {
      BlockStyle style;
      style.textIndentDefined = true;
      ParsedText text(hyphenation, false, style, 0);
      text.addWord("가나", EpdFontFamily::REGULAR, false, false, 100);
      text.addWord("다라마바사아", EpdFontFamily::REGULAR, false, false, 103);
      text.addWord("자", EpdFontFamily::REGULAR, false, false, 110);
      unsigned coverage[11] = {};
      text.layoutAndExtractLines(renderer, 0, width, [&](std::unique_ptr<TextBlock> block, auto) {
        ASSERT_TRUE(block->valid());
        for (uint16_t i = 0; i < block->wordCount(); ++i) {
          const auto range = block->wordSourceRange(i);
          ASSERT_GE(range.start, 100u);
          ASSERT_LE(range.end, 111u);
          ASSERT_LT(range.start, range.end);
          for (uint32_t offset = range.start; offset < range.end; ++offset) ++coverage[offset - 100];
        }
      });
      for (unsigned i = 0; i < 11; ++i) {
        EXPECT_EQ(coverage[i], i == 2 || i == 9 ? 0u : 1u) << "width=" << width << " offset=" << i;
      }
    }
  }
}

TEST(ClippingAnchors, NfcSourceCoverageSurvivesSplitsAndPartialExtraction) {
  Hyphenator::setPreferredLanguage("ko");
  GfxRenderer renderer;
  for (const bool focus : {false, true}) {
    BlockStyle style;
    style.textIndentDefined = true;
    ParsedText text(true, focus, style, 0);
    // Latin composition, focus punctuation, CJK tokenization, and Hangul L+V+T.
    text.addWord("Cafe\xCC\x81!中\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB國", EpdFontFamily::REGULAR, false, false, 100);
    text.addWord("끝", EpdFontFamily::REGULAR, false, false, 112);
    unsigned coverage[13] = {};
    auto inspect = [&](std::unique_ptr<TextBlock> block, auto) {
      ASSERT_TRUE(block->valid());
      for (uint16_t i = 0; i < block->wordCount(); ++i) {
        const auto range = block->wordSourceRange(i);
        ASSERT_GE(range.start, 100u);
        ASSERT_LE(range.end, 113u);
        ASSERT_LT(range.start, range.end);
        for (uint32_t offset = range.start; offset < range.end; ++offset) ++coverage[offset - 100];
      }
    };
    text.layoutAndExtractLines(renderer, 0, 32, inspect, false);
    text.layoutAndExtractLines(renderer, 0, 32, inspect);
    for (unsigned i = 0; i < 13; ++i) EXPECT_EQ(coverage[i], i == 11 ? 0u : 1u) << "offset=" << i;
  }
}

TEST(ClippingAnchors, NfcFocusSegmentsKeepOriginalOffsets) {
  GfxRenderer renderer;
  ParsedText text(false, true);
  text.addWord("Cafe\xCC\x81!a\xCC\x82\xCC\x81", EpdFontFamily::REGULAR, false, false, 100);
  const uint32_t starts[] = {100, 105, 106};
  const uint32_t ends[] = {105, 106, 109};
  unsigned words = 0;
  text.layoutAndExtractLines(renderer, 0, 200, [&](std::unique_ptr<TextBlock> block, auto) {
    for (uint16_t i = 0; i < block->wordCount(); ++i) {
      ASSERT_LT(words, 3u);
      EXPECT_EQ(block->wordSourceRange(i).start, starts[words]);
      EXPECT_EQ(block->wordSourceRange(i).end, ends[words]);
      ++words;
    }
  });
  EXPECT_EQ(words, 3u);
}

TEST(ClippingAnchors, NfcHyphenationMarksSurviveCacheRoundTrip) {
  Hyphenator::setPreferredLanguage("en");
  GfxRenderer renderer;
  const auto path = (std::filesystem::temp_directory_path() / "crosspoint-nfc-hyphens.bin").string();
  for (const bool focus : {false, true}) {
    BlockStyle style;
    style.textIndentDefined = true;
    ParsedText text(true, focus, style, 0);
    text.addWord("cafe\xCC\x81teria", EpdFontFamily::REGULAR, false, false, 100);
    uint32_t previousEnd = 100;
    unsigned hyphens = 0;
    text.layoutAndExtractLines(renderer, 0, 40, [&](std::unique_ptr<TextBlock> block, auto) {
      {
        HalFile file;
        ASSERT_TRUE(file.open(path.c_str(), "wb"));
        ASSERT_TRUE(block->serialize(file));
      }
      HalFile file;
      ASSERT_TRUE(file.open(path.c_str(), "rb"));
      auto restored = TextBlock::deserialize(file);
      ASSERT_NE(restored, nullptr);
      for (uint16_t i = 0; i < restored->wordCount(); ++i) {
        const auto range = restored->wordSourceRange(i);
        EXPECT_EQ(range.start, previousEnd);
        previousEnd = range.end;
        EXPECT_EQ(restored->wordStyle(i), block->wordStyle(i));
        EXPECT_EQ(restored->wordHasDiscretionaryHyphen(i), block->wordHasDiscretionaryHyphen(i));
        if (restored->wordHasDiscretionaryHyphen(i)) ++hyphens;
      }
    });
    EXPECT_EQ(previousEnd, 110u);
    EXPECT_GT(hyphens, 0u);
  }
  std::filesystem::remove(path);
}

TEST(ClippingAnchors, DenseNfdHangulSurvivesChunkRetirement) {
  Hyphenator::setPreferredLanguage("ko");
  GfxRenderer renderer;
  ParsedText text(true);
  std::string word;
  for (int i = 0; i < 22; ++i) word += "\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB";
  for (uint32_t i = 0; i < 6; ++i) text.addWord(word, EpdFontFamily::REGULAR, false, false, 100 + i * 67);
  uint32_t covered = 0;
  auto inspect = [&](std::unique_ptr<TextBlock> block, auto) {
    for (uint16_t i = 0; i < block->wordCount(); ++i) {
      const auto range = block->wordSourceRange(i);
      if (range.start >= 100000) {
        EXPECT_EQ(range.start, 100000u);
        EXPECT_EQ(range.end, 100005u);
      } else {
        EXPECT_EQ((range.start - 100) % 67 % 3, 0u);
        EXPECT_EQ((range.end - 100) % 67 % 3, 0u);
      }
      covered += range.end - range.start;
    }
  };
  text.layoutAndExtractLines(renderer, 0, 80, inspect, false);
  text.addWord("Cafe\xCC\x81", EpdFontFamily::REGULAR, false, false, 100000);
  text.layoutAndExtractLines(renderer, 0, 80, inspect);
  EXPECT_EQ(covered, 6u * 66 + 5);
}
