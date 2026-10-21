// UPSTREAM_ONLY — not registered in CT CMake.
// Synthetic solid-glyph metrics and turned-comma substitution are not in CT EpdFont ABI.

TEST(EpdFont, MissingSolidSymbolsHaveVisibleBounds) {
  // No M or replacement glyph: the fallback uses 3/4 of the 12px ascender.
  EXPECT_EQ(textWidth("█"), 9);
  EXPECT_EQ(textHeight("█"), 12);
  EXPECT_EQ(textWidth("███"), 27);
  EXPECT_EQ(textWidth("■"), 8);
  EXPECT_EQ(textHeight("■"), 10);
  EXPECT_EQ(textWidth("o█o"), 26);
}

TEST(EpdFont, SolidSymbolsUseEmAdvanceAndKeepRealGlyphs) {
  const EpdGlyph glyphs[] = {{10, 12, 168, 0, 12, 0, 0}, {3, 4, 64, 1, 4, 1, 0}};
  const EpdUnicodeInterval intervals[] = {{'M', 'M', 0}, {0x25A0, 0x25A0, 1}};
  EpdFontData data{};
  data.glyph = glyphs;
  data.intervals = intervals;
  data.intervalCount = 2;
  data.ascender = 12;
  const EpdFont font(&data);
  EpdGlyph fallback;
  const auto* block = font.getGlyphMetrics(0x2588, fallback);
  ASSERT_EQ(block, &fallback);
  EXPECT_EQ(block->advanceX, 168);
  EXPECT_EQ(block->width, 11);
  EXPECT_EQ(block->height, 12);
  EXPECT_EQ(font.getGlyphMetrics(0x25A0, fallback), &glyphs[1]);
}

TEST(EpdFont, SolidFallbackOverridesReplacementGlyph) {
  const EpdGlyph replacement{2, 3, 32, 0, 3, 1, 0};
  const EpdUnicodeInterval interval{0xFFFD, 0xFFFD, 0};
  EpdFontData data{};
  data.glyph = &replacement;
  data.intervals = &interval;
  data.intervalCount = 1;
  data.ascender = 12;
  const EpdFont font(&data);
  EpdGlyph fallback;
  EXPECT_EQ(font.getGlyphMetrics(0x2588, fallback), &fallback);
  EXPECT_EQ(fallback.advanceX, fp4::fromPixel(9));
  EXPECT_EQ(fallback.width, 9);
  EXPECT_EQ(fallback.height, 12);
  EXPECT_EQ(font.getGlyphMetrics('Z', fallback), &replacement);
}

TEST(EpdFont, SolidSymbolsRespectOnDemandFontCoverage) {
  const EpdGlyph realBlock{7, 9, 112, 0, 9, 1, 0};
  EpdFontData data{};
  data.ascender = 12;
  data.glyphMissCtx = const_cast<EpdGlyph*>(&realBlock);
  data.coverageHandler = [](void*, uint32_t cp) { return cp == 0x2588; };
  data.glyphMissHandler = [](void* ctx, uint32_t cp) -> const EpdGlyph* {
    return cp == 0x2588 ? static_cast<const EpdGlyph*>(ctx) : nullptr;
  };
  const EpdFont font(&data);
  EpdGlyph fallback;
  EXPECT_EQ(font.getGlyphMetrics(0x2588, fallback), &realBlock);
  EXPECT_EQ(font.getGlyphMetrics(0x25A0, fallback), &fallback);
}

TEST(EpdFont, EmptyFontStillMeasuresSolidSymbols) {
  const EpdFontData data{};
  const EpdFont font(&data);
  int width, height;
  font.getTextDimensions("██", &width, &height);
  EXPECT_EQ(width, 12);
  EXPECT_EQ(height, 8);
}

TEST(EpdFont, MissingTurnedCommaUsesQuoteAndNativeGlyphWins) {
  const EpdGlyph glyphs[] = {{3, 5, 64, 0, 8, 1, 0}, {4, 6, 80, 0, 9, 1, 1}};
  const EpdUnicodeInterval intervals[] = {{0x02BB, 0x02BB, 0}, {0x2018, 0x2018, 1}};
  EpdFontData data{};
  data.glyph = glyphs;
  data.intervals = intervals + 1;
  data.intervalCount = 1;
  const EpdFont font(&data);
  EXPECT_EQ(font.getGlyph(0x02BB), &glyphs[1]);
  data.intervals = intervals;
  data.intervalCount = 2;
  EXPECT_EQ(font.getGlyph(0x02BB), &glyphs[0]);
}
