// UPSTREAM_ONLY — not registered in CT CMake.
// Upstream inheritance/ancestor/font-size/border/whitespace and ordered-rule cascade model is not adopted; preserve CT CSS cache12 and bounded rules.

TEST_F(CssParserTest, ResolvesCaseInsensitiveCascadeAndRepeatedSelectors) {
  CssParser parser(cachePath());
  ASSERT_EQ(loadCss(parser,
                    "P { text-align: center; }\n"
                    ".Note { font-weight: bold; text-align: right; }\n"
                    "p.note { font-style: italic; text-align: justify; }\n"
                    ".note { margin-top: 2em; }\n"),
            CssParser::ParseResult::Complete);

  EXPECT_EQ(parser.ruleCount(), 4u);
  const CssStyle style = parser.resolveStyle("p", "NOTE");
  EXPECT_EQ(style.textAlign, CssTextAlign::Justify);
  EXPECT_EQ(style.fontWeight, CssFontWeight::Bold);
  EXPECT_EQ(style.fontStyle, CssFontStyle::Italic);
  ASSERT_TRUE(style.hasMarginTop());
  EXPECT_FLOAT_EQ(style.marginTop.value, 2.0f);
  EXPECT_EQ(style.marginTop.unit, CssUnit::Em);
}

TEST_F(CssParserTest, FontInheritanceSurvivesCascadeAndCacheRoundTrip) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer,
                    "p { font-weight:bold; font-style:italic; }"
                    "#inherited { font-weight:inherit; font-style:INHERIT; }"
                    ".inherited { font-weight:normal; font-style:normal; }"
                    ".last { font-weight:bold; font-weight:inherit; font-style:italic; font-style:inherit; }"
                    ".normal { font-weight:inherit; font-weight:normal; font-style:inherit; font-style:normal; }"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));
  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  for (const auto* parser : {&writer, &reader}) {
    for (const auto style : {parser->resolveStyle("p", "inherited", "inherited"), parser->resolveStyle("p", "last")}) {
      EXPECT_TRUE(style.hasFontStyle());
      EXPECT_TRUE(style.hasFontWeight());
      EXPECT_EQ(style.fontStyle, CssFontStyle::Inherit);
      EXPECT_EQ(style.fontWeight, CssFontWeight::Inherit);
    }
    const auto normal = parser->resolveStyle("p", "normal");
    EXPECT_EQ(normal.fontStyle, CssFontStyle::Normal);
    EXPECT_EQ(normal.fontWeight, CssFontWeight::Normal);
  }
  const auto inlineStyle = CssParser::parseInlineStyle("font-weight:bold;font-weight:inherit;font-style:inherit");
  EXPECT_EQ(inlineStyle.fontWeight, CssFontWeight::Inherit);
  EXPECT_EQ(inlineStyle.fontStyle, CssFontStyle::Inherit);
}

TEST_F(CssParserTest, FontInheritanceCacheRejectsOldVersion) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer, "p{font-weight:inherit;font-style:inherit}"), CssParser::ParseResult::Complete);
  for (const bool complete : {false, true}) {
    ASSERT_TRUE(writer.saveToCache(complete));
    auto bytes = readCache();
    ASSERT_FALSE(bytes.empty());
    bytes[0] = CssParser::CSS_CACHE_VERSION - 1;
    writeCache(bytes);
    CssParser reader(cachePath());
    EXPECT_EQ(reader.inspectCache(), CssParser::CacheStatus::Invalid);
    EXPECT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Invalid);
  }
}

TEST_F(CssParserTest, ParsesFontSizeUnitsAndKeywords) {
  CssParser parser(cachePath());
  ASSERT_EQ(loadCss(parser,
                    ".em { font-size: 1.5em; }\n"
                    ".pct { font-size: 80%; }\n"
                    ".px { font-size: 24px; }\n"
                    ".pt { font-size: 18pt; }\n"
                    ".kw { font-size: x-large; }\n"
                    ".rel { font-size: smaller; }\n"
                    ".bad { font-size: 2vw; }\n"),
            CssParser::ParseResult::Complete);

  const auto size = [&](const char* cls) { return parser.resolveStyle("p", cls).fontSize; };
  EXPECT_FLOAT_EQ(size("em").value, 1.5f);
  EXPECT_EQ(size("em").unit, CssUnit::Em);
  EXPECT_FLOAT_EQ(size("pct").value, 0.8f);
  EXPECT_EQ(size("pct").unit, CssUnit::Em);
  EXPECT_FLOAT_EQ(size("px").value, 1.5f);
  EXPECT_EQ(size("px").unit, CssUnit::Rem);
  EXPECT_FLOAT_EQ(size("pt").value, 1.5f);
  EXPECT_FLOAT_EQ(size("kw").value, 1.5f);
  EXPECT_EQ(size("kw").unit, CssUnit::Rem);
  EXPECT_FLOAT_EQ(size("rel").value, 0.83f);
  EXPECT_FALSE(parser.resolveStyle("p", "bad").hasFontSize());
}

TEST_F(CssParserTest, MatchesIdAndCompoundClassSelectors) {
  CssParser parser(cachePath());
  ASSERT_EQ(loadCss(parser,
                    "#title { text-align: center; }\n"
                    "h1#title { font-weight: bold; }\n"
                    ".first.noindent { text-indent: 0; }\n"
                    "p.b.a { font-style: italic; }\n"
                    "*.star { text-align: right; }\n"),
            CssParser::ParseResult::Complete);

  const CssStyle title = parser.resolveStyle("h1", "", "Title");
  EXPECT_EQ(title.textAlign, CssTextAlign::Center);
  EXPECT_EQ(title.fontWeight, CssFontWeight::Bold);
  EXPECT_FALSE(parser.resolveStyle("h2", "", "title").hasFontWeight());

  EXPECT_TRUE(parser.resolveStyle("p", "noindent other first").hasTextIndent());
  EXPECT_FALSE(parser.resolveStyle("p", "first").hasTextIndent());
  EXPECT_EQ(parser.resolveStyle("p", "a b").fontStyle, CssFontStyle::Italic);
  EXPECT_FALSE(parser.resolveStyle("div", "a b").hasFontStyle());
  EXPECT_EQ(parser.resolveStyle("div", "star").textAlign, CssTextAlign::Right);
}

TEST_F(CssParserTest, MatchesDescendantAndChildSelectorsAgainstAncestors) {
  CssParser parser(cachePath());
  ASSERT_EQ(loadCss(parser,
                    "p { text-align: justify; }\n"
                    ".poem p { text-align: left; }\n"
                    "blockquote > p { font-style: italic; }\n"
                    "p + p { font-weight: bold; }\n"
                    "a:hover { font-weight: bold; }\n"),
            CssParser::ParseResult::Complete);
  EXPECT_EQ(parser.ruleCount(), 3u);

  const CssAncestor body = CssParser::makeAncestor("body", "", "");
  const CssAncestor poem = CssParser::makeAncestor("div", "Poem stanza", "");
  const CssAncestor quote = CssParser::makeAncestor("blockquote", "", "");

  const CssAncestor inPoem[] = {body, poem, CssParser::makeAncestor("div", "", "")};
  EXPECT_EQ(parser.resolveStyle("p", "", "", inPoem, 3).textAlign, CssTextAlign::Left);

  const CssAncestor plain[] = {body};
  EXPECT_EQ(parser.resolveStyle("p", "", "", plain, 1).textAlign, CssTextAlign::Justify);
  EXPECT_EQ(parser.resolveStyle("p", "").textAlign, CssTextAlign::Justify);

  const CssAncestor directQuote[] = {body, quote};
  EXPECT_EQ(parser.resolveStyle("p", "", "", directQuote, 2).fontStyle, CssFontStyle::Italic);
  const CssAncestor nestedQuote[] = {body, quote, poem};
  EXPECT_FALSE(parser.resolveStyle("p", "", "", nestedQuote, 3).hasFontStyle());
  EXPECT_FALSE(parser.resolveStyle("p", "", "", nestedQuote, 3).hasFontWeight());
}

TEST_F(CssParserTest, AppliesRulesInSpecificityOrder) {
  CssParser parser(cachePath());
  ASSERT_EQ(loadCss(parser,
                    "#x { text-align: right; }\n"
                    ".c { text-align: center; }\n"
                    "div p { text-align: left; }\n"
                    "p { text-align: justify; }\n"),
            CssParser::ParseResult::Complete);
  const CssAncestor ancestors[] = {CssParser::makeAncestor("div", "", "")};
  EXPECT_EQ(parser.resolveStyle("p", "", "", ancestors, 1).textAlign, CssTextAlign::Left);
  EXPECT_EQ(parser.resolveStyle("p", "c", "", ancestors, 1).textAlign, CssTextAlign::Center);
  EXPECT_EQ(parser.resolveStyle("p", "c", "x", ancestors, 1).textAlign, CssTextAlign::Right);
}

TEST_F(CssParserTest, EqualSpecificityUsesSourceOrderAcrossStylesheetsAndCache) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer, ".z { font-weight: bold; } .a { font-weight: normal; }"), CssParser::ParseResult::Complete);
  for (const char* classes : {"z a", "a z"}) {
    EXPECT_EQ(writer.resolveStyle("p", classes).fontWeight, CssFontWeight::Normal);
  }
  // A later rule for .z must not move its earlier font-weight declaration past .a.
  ASSERT_EQ(loadCss(writer, ".z { font-style: italic; } div p { text-align: right; } body p { text-align: left; }"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));
  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  const CssAncestor ancestors[] = {CssParser::makeAncestor("body", "", ""), CssParser::makeAncestor("div", "", "")};
  for (const CssParser* parser : {&writer, &reader}) {
    for (const char* classes : {"z a", "a z"}) {
      const auto style = parser->resolveStyle("p", classes, "", ancestors, 2);
      EXPECT_EQ(style.fontWeight, CssFontWeight::Normal);
      EXPECT_EQ(style.fontStyle, CssFontStyle::Italic);
      EXPECT_EQ(style.textAlign, CssTextAlign::Left);
    }
  }
  ASSERT_EQ(loadCss(reader, ".z { font-weight: bold; }"), CssParser::ParseResult::Complete);
  for (const char* classes : {"z a", "a z"}) {
    EXPECT_EQ(reader.resolveStyle("p", classes).fontWeight, CssFontWeight::Bold);
  }
}

TEST_F(CssParserTest, RepeatedRulesBeyondOneMatchBatchKeepEarlierProperties) {
  CssParser writer(cachePath());
  std::string css = ".z { font-style: italic; }";
  for (int i = 0; i < 20; ++i) {
    css += ".z { font-weight: bold; } .a { font-weight: normal; }";
  }
  ASSERT_EQ(loadCss(writer, css), CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));
  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  for (const CssParser* parser : {&writer, &reader}) {
    for (const char* classes : {"z a", "a z"}) {
      const auto style = parser->resolveStyle("p", classes);
      EXPECT_EQ(style.fontWeight, CssFontWeight::Normal);
      EXPECT_EQ(style.fontStyle, CssFontStyle::Italic);
    }
  }
}

TEST_F(CssParserTest, CacheRoundTripsContextualAndFontSizeRules) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer, ".poem p { font-size: 0.9em; }\n#t.a.b { font-weight: bold; }\n"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));

  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  const CssAncestor ancestors[] = {CssParser::makeAncestor("div", "poem", "")};
  const CssStyle style = reader.resolveStyle("p", "", "", ancestors, 1);
  EXPECT_TRUE(style.hasFontSize());
  EXPECT_FLOAT_EQ(style.fontSize.value, 0.9f);
  EXPECT_EQ(reader.resolveStyle("span", "b a", "t").fontWeight, CssFontWeight::Bold);
}

TEST_F(CssParserTest, ParsesSmallCapsAndForcedPageBreaks) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer,
                    ".sc { font-variant: small-caps; }\n"
                    ".asc { font-variant-caps: all-small-caps; }\n"
                    ".normal { font-variant: normal; }\n"
                    ".before { page-break-before: always; }\n"
                    ".after { break-after: right; }\n"
                    ".avoid { page-break-before: avoid; }\n"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));

  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  EXPECT_TRUE(reader.resolveStyle("span", "sc").smallCaps);
  EXPECT_TRUE(reader.resolveStyle("span", "asc").smallCaps);
  const CssStyle normal = reader.resolveStyle("span", "normal");
  EXPECT_TRUE(normal.hasSmallCaps());
  EXPECT_FALSE(normal.smallCaps);
  EXPECT_TRUE(reader.resolveStyle("div", "before").pageBreakBefore);
  EXPECT_TRUE(reader.resolveStyle("div", "after").pageBreakAfter);
  const CssStyle avoid = reader.resolveStyle("div", "avoid");
  EXPECT_TRUE(avoid.hasPageBreakBefore());
  EXPECT_FALSE(avoid.pageBreakBefore);
}

TEST_F(CssParserTest, ParsesBordersAndBackgroundShade) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer,
                    ".all { border: 1px solid #000; }\n"
                    ".top { border-top: thick double; }\n"
                    ".edges { border-width: 2px 0; border-style: solid; }\n"
                    ".nostyle { border-bottom: 1px; }\n"
                    ".long { border-left-width: 3pt; border-left-style: dashed; }\n"
                    ".gray { background-color: #eee; }\n"
                    ".white { background-color: #fff; }\n"
                    ".dark { background: black; }\n"
                    ".rgb { background: rgb(200, 200, 200) url(x.png) no-repeat; }\n"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));

  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  const CssStyle all = reader.resolveStyle("div", "all");
  EXPECT_TRUE(all.borderTop.visible() && all.borderRight.visible() && all.borderBottom.visible() &&
              all.borderLeft.visible());
  EXPECT_EQ(all.borderLeft.width, 1);
  EXPECT_EQ(all.borderLeft.style, CssBorderStyle::Solid);

  const CssStyle top = reader.resolveStyle("div", "top");
  EXPECT_EQ(top.borderTop.width, 3);
  EXPECT_EQ(top.borderTop.style, CssBorderStyle::Double);
  EXPECT_FALSE(top.borderBottom.visible());

  const CssStyle edges = reader.resolveStyle("div", "edges");
  EXPECT_TRUE(edges.borderTop.visible());
  EXPECT_TRUE(edges.borderBottom.visible());
  EXPECT_FALSE(edges.borderLeft.visible());

  EXPECT_FALSE(reader.resolveStyle("div", "nostyle").hasVisibleBorder());
  const CssStyle dashed = reader.resolveStyle("div", "long");
  EXPECT_EQ(dashed.borderLeft.width, 4);
  EXPECT_EQ(dashed.borderLeft.style, CssBorderStyle::Dashed);

  EXPECT_TRUE(reader.resolveStyle("div", "gray").shaded);
  EXPECT_FALSE(reader.resolveStyle("div", "white").shaded);
  EXPECT_FALSE(reader.resolveStyle("div", "dark").shaded);
  EXPECT_TRUE(reader.resolveStyle("div", "rgb").shaded);
}

TEST_F(CssParserTest, StoresFirstLetterRulesSeparately) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer,
                    "p::first-letter { font-size: 3em; float: left; }\n"
                    ".chapter p:first-letter { initial-letter: 4 3; }\n"
                    "p { text-indent: 1em; }\n"
                    "p::before { content: 'x'; font-weight: bold; }\n"),
            CssParser::ParseResult::Complete);
  EXPECT_TRUE(writer.hasFirstLetterRules());
  ASSERT_TRUE(writer.saveToCache(true));

  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  EXPECT_TRUE(reader.hasFirstLetterRules());
  const CssStyle paragraph = reader.resolveStyle("p", "");
  EXPECT_TRUE(paragraph.hasTextIndent());
  EXPECT_FALSE(paragraph.hasFontSize());
  EXPECT_FALSE(paragraph.hasFontWeight());

  const CssStyle letter = reader.resolveStyle("p", "", "", nullptr, 0, true);
  EXPECT_TRUE(letter.floatLeft);
  EXPECT_FLOAT_EQ(letter.fontSize.value, 3.0f);
  EXPECT_FALSE(letter.hasTextIndent());
  EXPECT_EQ(letter.initialLetter, 0);

  const CssAncestor chapter[] = {CssParser::makeAncestor("div", "chapter", "")};
  EXPECT_EQ(reader.resolveStyle("p", "", "", chapter, 1, true).initialLetter, 4);
}

TEST_F(CssParserTest, PreservedWhitespaceSurvivesCacheAndNormalOverridesIt) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer, ".mono { white-space: pre-wrap; } .normal { white-space: normal; }"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));
  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  auto style = reader.resolveStyle("p", "mono");
  ASSERT_TRUE(style.defined.whiteSpace);
  EXPECT_TRUE(style.preserveWhitespace);
  style.applyOver(reader.resolveStyle("span", "normal"));
  EXPECT_FALSE(style.preserveWhitespace);
}

TEST_F(CssParserTest, WhiteSpacePreservationValuesAndUnsupportedOverrides) {
  for (const char* value : {"pre", "pre-wrap", "break-spaces", "PRE", "BREAK-SPACES !important"}) {
    const std::string declaration = std::string("white-space: ") + value;
    auto style = CssParser::parseInlineStyle(declaration);
    EXPECT_TRUE(style.defined.whiteSpace) << value;
    EXPECT_TRUE(style.preserveWhitespace) << value;
    style.applyOver(CssParser::parseInlineStyle("white-space: normal"));
    EXPECT_TRUE(style.defined.whiteSpace);
    EXPECT_FALSE(style.preserveWhitespace);
  }
  const auto unsupported = CssParser::parseInlineStyle("white-space: nowrap");
  EXPECT_FALSE(unsupported.defined.whiteSpace);
  auto style = CssParser::parseInlineStyle("white-space: pre; white-space: nowrap");
  EXPECT_TRUE(style.defined.whiteSpace);
  EXPECT_TRUE(style.preserveWhitespace);
  style.applyOver(unsupported);
  EXPECT_TRUE(style.preserveWhitespace);
}

TEST_F(CssParserTest, BorderComponentsCascadeIndependentlyAndSurviveCache) {
  CssParser writer(cachePath());
  ASSERT_EQ(loadCss(writer,
                    "p { border: 1px solid; } .wide { border-width: 2px; } "
                    "div { border-width: 3px; } .solid { border-style: solid; } "
                    ".top { border-top-width: 4px; border-left-style: dashed; } "
                    ".none { border-style: none; }"),
            CssParser::ParseResult::Complete);
  ASSERT_TRUE(writer.saveToCache(true));
  CssParser reader(cachePath());
  ASSERT_EQ(reader.loadFromCache(), CssParser::CacheLoadResult::Complete);
  for (const CssParser* parser : {&writer, &reader}) {
    const auto wide = parser->resolveStyle("p", "wide");
    EXPECT_TRUE(wide.hasVisibleBorder());
    EXPECT_EQ(wide.borderTop.width, 2);
    EXPECT_EQ(wide.borderTop.style, CssBorderStyle::Solid);
    const auto solid = parser->resolveStyle("div", "solid");
    EXPECT_TRUE(solid.hasVisibleBorder());
    EXPECT_EQ(solid.borderLeft.width, 3);
    const auto top = parser->resolveStyle("p", "top");
    EXPECT_EQ(top.borderTop.width, 4);
    EXPECT_EQ(top.borderTop.style, CssBorderStyle::Solid);
    EXPECT_EQ(top.borderLeft.width, 1);
    EXPECT_EQ(top.borderLeft.style, CssBorderStyle::Dashed);
    EXPECT_FALSE(parser->resolveStyle("p", "none").hasVisibleBorder());
  }
  const auto inlineStyle = CssParser::parseInlineStyle("border-style: solid; border-width: 2px");
  EXPECT_TRUE(inlineStyle.hasVisibleBorder());
  EXPECT_EQ(inlineStyle.borderTop.width, 2);
}

TEST_F(CssParserTest, DescendantRetriesWholeChildChain) {
  CssParser parser(cachePath());
  ASSERT_EQ(loadCss(parser, "body > div p { font-weight: bold; } body > div > p { font-style: italic; }"),
            CssParser::ParseResult::Complete);
  const CssAncestor ancestors[] = {CssParser::makeAncestor("body", "", ""), CssParser::makeAncestor("div", "", ""),
                                   CssParser::makeAncestor("div", "", "")};
  const auto style = parser.resolveStyle("p", "", "", ancestors, 3);
  EXPECT_EQ(style.fontWeight, CssFontWeight::Bold);
  EXPECT_FALSE(style.hasFontStyle());
}
