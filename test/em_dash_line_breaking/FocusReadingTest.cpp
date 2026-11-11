#include <Epub/ParsedText.h>
#include <Epub/blocks/TextBlock.h>
#include <GfxRenderer.h>
#include <MiniBidi/BidiUtils.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>

TEST(FocusReading, PreservesLetterOrderAndRunPlacement) {
  for (const bool rtlParagraph : {false, true}) {
    for (const int8_t tracking : {0, 2}) {
      for (const bool focus : {false, true}) {
        for (const auto style : {EpdFontFamily::REGULAR, EpdFontFamily::ITALIC, EpdFontFamily::BOLD}) {
          for (const char* word : {"שלום", "בראשית", "התלמידים", "reading", "١٢٣٤"}) {
            SCOPED_TRACE(word);
            GfxRenderer renderer;
            renderer.boldGlyphAdvance = 11;
            BlockStyle blockStyle;
            blockStyle.isRtl = rtlParagraph;
            blockStyle.directionDefined = true;
            blockStyle.characterSpacing = tracking;
            ParsedText text(false, focus, blockStyle, 0);
            text.addWord(word, style);
            text.layoutAndExtractLines(
                renderer, 0, 400, [&](auto block, auto) { block->render(renderer, 0, 0, 0); }, true, tracking);
            ASSERT_FALSE(renderer.textDraws.empty());
            std::sort(renderer.textDraws.begin(), renderer.textDraws.end(),
                      [](const auto& a, const auto& b) { return a.x < b.x; });
            std::string actual;
            int nextX = renderer.textDraws.front().x;
            for (const auto& draw : renderer.textDraws) {
              EXPECT_EQ(draw.x, nextX);
              nextX += renderer.getTextAdvanceX(0, draw.text.c_str(), draw.style, tracking) + tracking;
              std::string visual;
              ASSERT_TRUE(BidiUtils::applyBidiVisual(draw.text.c_str(), visual, static_cast<int>(draw.direction)));
              actual += visual;
            }
            std::string expected;
            ASSERT_TRUE(BidiUtils::applyBidiVisual(word, expected));
            EXPECT_EQ(actual, expected);
            if (focus && style == EpdFontFamily::REGULAR) {
              ASSERT_EQ(renderer.textDraws.size(), 2u);
              const bool rtlWord = BidiUtils::startsWithRtl(word);
              EXPECT_TRUE(renderer.textDraws[rtlWord ? 1 : 0].style & EpdFontFamily::BOLD);
              EXPECT_FALSE(renderer.textDraws[rtlWord ? 0 : 1].style & EpdFontFamily::BOLD);
              if (style == EpdFontFamily::ITALIC) {
                for (const auto& draw : renderer.textDraws) EXPECT_TRUE(draw.style & EpdFontFamily::ITALIC);
              }
            }
          }
        }
      }
    }
  }
}

TEST(BionicReading, AllStagesUnicodeAndBoundedPrefix) {
  const size_t expected[] = {2, 2, 3, 3, 4, 4};
  for (uint8_t stage = 0; stage < 6; ++stage) {
    for (const char* word : {"abcdefgh", "가나다라마바사아", "가나ab다라cd"}) {
      GfxRenderer renderer;
      ParsedText text(false, true, BlockStyle{}, 0, stage);
      text.addWord(word, EpdFontFamily::REGULAR);
      text.layoutAndExtractLines(renderer, 0, 1000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
      size_t boldCount = 0;
      std::string joined;
      for (const auto& draw : renderer.textDraws) {
        joined += draw.text;
        if (draw.style & EpdFontFamily::BOLD) {
          for (const unsigned char c : draw.text)
            if ((c & 0xc0) != 0x80) ++boldCount;
        }
      }
      EXPECT_EQ(joined, word);
      // The default English stage preserves its former 45% floor result; Hangul uses ceil.
      EXPECT_EQ(boldCount, stage == 5 && std::string_view(word) == "abcdefgh" ? 3u : expected[stage]);
    }
  }
  for (const char* word : {"a", "가", "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"}) {
    GfxRenderer renderer;
    ParsedText text(false, true, BlockStyle{}, 0, 5);
    text.addWord(word, EpdFontFamily::REGULAR);
    text.layoutAndExtractLines(renderer, 0, 2000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
    size_t boldCount = 0;
    for (const auto& draw : renderer.textDraws)
      if (draw.style & EpdFontFamily::BOLD)
        for (const unsigned char c : draw.text)
          if ((c & 0xc0) != 0x80) ++boldCount;
    EXPECT_EQ(boldCount, std::string(word).size() > 10 ? 9u : 1u);
  }
}

TEST(BionicReading, OffAndProtectedFragmentsStayUnchanged) {
  for (const auto style :
       {EpdFontFamily::REGULAR, EpdFontFamily::ITALIC, EpdFontFamily::UNDERLINE, EpdFontFamily::BOLD}) {
    for (const bool enabled : {false, true}) {
      GfxRenderer renderer;
      ParsedText text(false, enabled, BlockStyle{}, 0, 0);
      text.addWord("reading", style, false, false, 0, 0, 0, false, false);
      text.addWord("continued", style, false, true);
      text.layoutAndExtractLines(renderer, 0, 1000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
      for (const auto& draw : renderer.textDraws) EXPECT_EQ(draw.style, style);
    }
  }
}

TEST(BionicReading, EnglishDefaultKeepsLegacyFloorAndBounds) {
  struct Case {
    const char* word;
    size_t expectedBold;
  };
  const Case cases[] = {{"a", 1}, {"read", 1}, {"reading", 3}, {"supercalifragilistic", 9}};
  for (const auto& testCase : cases) {
    GfxRenderer renderer;
    ParsedText text(false, true, BlockStyle{}, 0, 5);
    text.addWord(testCase.word, EpdFontFamily::REGULAR);
    text.layoutAndExtractLines(renderer, 0, 2000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
    size_t boldCount = 0;
    for (const auto& draw : renderer.textDraws) {
      if ((draw.style & EpdFontFamily::BOLD) == 0) continue;
      for (const unsigned char c : draw.text)
        if ((c & 0xc0) != 0x80) ++boldCount;
    }
    EXPECT_EQ(boldCount, testCase.expectedBold) << testCase.word;
  }
}

TEST(BionicReading, EnglishStrengthAndMixedPunctuation) {
  {
    GfxRenderer renderer;
    ParsedText text(false, true, BlockStyle{}, 0, 0);
    text.addWord("reading", EpdFontFamily::REGULAR);
    text.layoutAndExtractLines(renderer, 0, 2000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
    size_t boldCount = 0;
    for (const auto& draw : renderer.textDraws) {
      if ((draw.style & EpdFontFamily::BOLD) == 0) continue;
      for (const unsigned char c : draw.text)
        if ((c & 0xc0) != 0x80) ++boldCount;
    }
    EXPECT_EQ(boldCount, 2u);  // 20% with ceil for a user-selected non-default stage.
  }

  const char* sentence = "Reading, 가나다라 world!";
  GfxRenderer renderer;
  ParsedText text(false, true, BlockStyle{}, 0, 5);
  text.addWord(sentence, EpdFontFamily::REGULAR);
  text.layoutAndExtractLines(renderer, 0, 2000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
  std::string joined;
  std::string bold;
  for (const auto& draw : renderer.textDraws) {
    joined += draw.text;
    if (draw.style & EpdFontFamily::BOLD) bold += draw.text;
  }
  EXPECT_EQ(joined, sentence);
  EXPECT_EQ(bold, "Rea가나wo");
}

TEST(BionicReading, EnglishAuthorStylesAndInlineFragmentsAreProtected) {
  for (const auto style : {EpdFontFamily::ITALIC, EpdFontFamily::UNDERLINE, EpdFontFamily::BOLD}) {
    GfxRenderer renderer;
    ParsedText text(false, true, BlockStyle{}, 0, 5);
    text.addWord("reading", style, style == EpdFontFamily::UNDERLINE);
    text.layoutAndExtractLines(renderer, 0, 2000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
    ASSERT_EQ(renderer.textDraws.size(), 1u);
    EXPECT_EQ(renderer.textDraws.front().style,
              style == EpdFontFamily::UNDERLINE ? EpdFontFamily::UNDERLINE : style);
  }

  GfxRenderer renderer;
  ParsedText text(false, true, BlockStyle{}, 0, 5);
  text.addWord("rea", EpdFontFamily::REGULAR, false, false, 0, 0, 0, false, false);
  text.addWord("ding", EpdFontFamily::ITALIC, false, true, 0, 0, 0, true, false);
  text.layoutAndExtractLines(renderer, 0, 2000, [&](auto line, auto) { line->render(renderer, 0, 0, 0); });
  std::string joined;
  for (const auto& draw : renderer.textDraws) {
    EXPECT_FALSE(draw.style & EpdFontFamily::BOLD);
    joined += draw.text;
  }
  EXPECT_EQ(joined, "reading");
}

TEST(BionicReading, HangulWrapDoesNotRestartEmphasis) {
  GfxRenderer renderer;
  ParsedText text(true, true, BlockStyle{}, 0, 5);
  text.addWord("가나다라마바사아", EpdFontFamily::REGULAR);
  unsigned lines = 0;
  text.layoutAndExtractLines(renderer, 0, 24, [&](auto line, auto) {
    ++lines;
    line->render(renderer, 0, 0, 0);
  });
  EXPECT_GT(lines, 1u);
  std::string all, bold;
  for (const auto& draw : renderer.textDraws) {
    all += draw.text;
    if (draw.style & EpdFontFamily::BOLD) bold += draw.text;
  }
  EXPECT_EQ(all, "가나다라마바사아");
  EXPECT_EQ(bold, "가나다라");
}
