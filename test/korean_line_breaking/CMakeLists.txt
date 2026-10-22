add_executable(KoreanLineBreakingTest
  KoreanLineBreakingTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/ParsedText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/WordStore.cpp
  ${REPO_ROOT}/lib/Epub/Epub/blocks/TextBlock.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/Hyphenator.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/LanguageRegistry.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/LiangHyphenation.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/HyphenationCommon.cpp
  ${REPO_ROOT}/lib/MiniBidi/BidiUtils.cpp
  ${REPO_ROOT}/lib/MiniBidi/minibidi.c
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(KoreanLineBreakingTest PRIVATE
  ${REPO_ROOT}/test/chapter_html_slim_parser/stubs
  ${REPO_ROOT}/lib
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/MiniBidi
  ${REPO_ROOT}/lib/FsHelpers
)

target_link_libraries(KoreanLineBreakingTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(KoreanLineBreakingTest)

# Exercise the production scanner directly without adding a firmware API.
production_slice(CJK_PREDICATE lib/Epub/Epub/ParsedText.cpp
  "bool hasCjkBreakOpportunityBetween(" "// Line-end split points")
production_slice(CJK_SCAN lib/Epub/Epub/ParsedText.cpp
  "std::vector<size_t> cjkCharacterBreakByteOffsets(" "int computeJustifyExtra(")
configure_file(CjkOffsetsTest.cpp.in CjkOffsetsTest.cpp @ONLY)
add_executable(CjkOffsetsTest ${CMAKE_CURRENT_BINARY_DIR}/CjkOffsetsTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp)
target_include_directories(CjkOffsetsTest PRIVATE ${REPO_ROOT}/lib/Epub/Epub ${REPO_ROOT}/lib/Utf8)
target_link_libraries(CjkOffsetsTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(CjkOffsetsTest)
