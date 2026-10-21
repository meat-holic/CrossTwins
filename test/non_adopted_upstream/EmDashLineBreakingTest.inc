// UPSTREAM_ONLY: CT preserves its existing renderer. The first case differs only in token grouping.
// The remaining three cases also differ in line placement in the fixed-width Host fixture;
// these are pre-existing CT limitations and unadopted upstream improvements, not new regressions.
// Do not change Korean/layout production code to force these expectations.

TEST(EmDashLineBreaking, HyphenationOffKeepsTogetherOnSameLineWithoutGap) {
  // 80 px fits "foo—" (32 px) and "bar" (24 px).
  const auto lines = layout({"foo—bar"}, /*hyphenation=*/false, 80);
  const std::vector<std::vector<std::string>> expected{{"foo—", "bar"}};
  ASSERT_EQ(wordsOf(lines), expected);
  // "bar" attaches directly to "foo—" with kerning 0, so xpos is exactly 0 and 32.
  EXPECT_EQ(lines[0].xpos, (std::vector<int16_t>{0, 32}));
}

TEST(EmDashLineBreaking, MultiEmDashStaysTogether) {
  // "foo——" is 5 chars * 8 px = 40 px. Width 45 fits "foo——", but not "bar".
  const auto lines = layout({"foo——bar"}, /*hyphenation=*/false, 45);
  const std::vector<std::vector<std::string>> expected{{"foo——"}, {"bar"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(EmDashLineBreaking, MixedCjkAndLatinSplitsAtDash) {
  // "漢" (8 px), "字—" (16 px), "bar" (24 px). Width 30 px fits "漢字—" (24 px), but not "bar".
  const auto lines = layout({"漢字—bar"}, /*hyphenation=*/false, 30);
  const std::vector<std::vector<std::string>> expected{{"漢", "字—"}, {"bar"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(EmDashLineBreaking, MixedCjkMultiEmDashStaysTogether) {
  // "漢" (8 px), "字——" (24 px), "bar" (24 px). Width 35 px fits "漢字——", but wraps "bar".
  const auto lines = layout({"漢字——bar"}, /*hyphenation=*/false, 35);
  const std::vector<std::vector<std::string>> expected{{"漢", "字——"}, {"bar"}};
  EXPECT_EQ(wordsOf(lines), expected);
}
