add_executable(ChapterHtmlSlimParserTest
  ChapterHtmlSlimParserTest.cpp
  SoftFlushTest.cpp
  ParserLinkStubs.cpp
  DictionaryHtmlPagesTest.cpp
  ${REPO_ROOT}/src/util/DictHtmlPages.cpp
  ${REPO_ROOT}/lib/Epub/Epub/Page.cpp
  ${REPO_ROOT}/lib/Epub/Epub/blocks/TextBlock.cpp
  ${REPO_ROOT}/lib/MiniBidi/BidiUtils.cpp
  ${REPO_ROOT}/lib/MiniBidi/minibidi.c
  ${REPO_ROOT}/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/css/CssParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/ParsedText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/WordStore.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageDimsProbe.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(ChapterHtmlSlimParserTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Epub/Epub/parsers
  ${REPO_ROOT}/lib/Epub/Epub/css
  ${REPO_ROOT}/lib/AsciiText
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/MiniBidi
  ${REPO_ROOT}/lib/FsHelpers
  ${REPO_ROOT}/lib/XmlParserUtils
)

target_link_libraries(ChapterHtmlSlimParserTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  expat
)

gtest_discover_tests(ChapterHtmlSlimParserTest)

# Reuse the Slice 8 CT dictionary contract against the same production parser.
get_target_property(CHAPTER_CONTRACT_SOURCES ChapterHtmlSlimParserTest SOURCES)
list(REMOVE_ITEM CHAPTER_CONTRACT_SOURCES ChapterHtmlSlimParserTest.cpp SoftFlushTest.cpp DictionaryHtmlPagesTest.cpp)
get_target_property(CHAPTER_CONTRACT_INCLUDES ChapterHtmlSlimParserTest INCLUDE_DIRECTORIES)
add_executable(DictionaryHtmlContractTest
  ${REPO_ROOT}/test/reader_ui_contract/DictionaryHtmlContractTest.cpp
  ${CHAPTER_CONTRACT_SOURCES})
target_include_directories(DictionaryHtmlContractTest PRIVATE ${CHAPTER_CONTRACT_INCLUDES})
target_link_libraries(DictionaryHtmlContractTest PRIVATE crosspoint_test_common GTest::gtest_main expat)
gtest_discover_tests(DictionaryHtmlContractTest)
