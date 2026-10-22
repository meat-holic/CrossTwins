// Reference only: original four PR #4126 tests, head c69984af65e0323bf1b3b2b0c29fcce5bdce4223.
// See PR4126-Test-Port.ko.md. No disabled/skipped active tests.
TEST(KoreanLineBreaking, MixedScriptsKeepBoundariesTouchingHangulTogether) {
  const auto lines = layout({"ab中가나國cd文"}, false, 300);
  const std::vector<std::vector<std::string>> expected{{"ab", "中가나國", "cd", "文"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, ExplicitHyphensBeforeFirstCjkCharacterRemainBreaks) {
  const auto lines = layout({"ab-cd가-나中"}, false, 300);
  const std::vector<std::vector<std::string>> expected{{"ab-", "cd가-", "나中"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, LatinHyphenRunsKeepTheNonCjkPath) {
  const auto lines = layout({"ab--cd"}, false, 300);
  const std::vector<std::vector<std::string>> expected{{"ab--", "cd"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, CombiningMarksAndCjkPunctuationStayAttached) {
  const auto lines = layout({"（中）文\xCC\x81字"}, false, 300);
  const std::vector<std::vector<std::string>> expected{{"（中）", "文\xCC\x81", "字"}};
  EXPECT_EQ(wordsOf(lines), expected);
}
