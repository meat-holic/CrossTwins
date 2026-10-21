production_slice(VERSIONS lib/Epub/Epub/Section.cpp
  "constexpr uint8_t SECTION_FILE_VERSION" "}  // namespace")
production_slice(WRITE_HEADER lib/Epub/Epub/Section.cpp
  "bool Section::writeSectionFileHeader" "bool Section::loadSectionFile")
production_slice(LOAD lib/Epub/Epub/Section.cpp
  "bool Section::loadSectionFile" "// Your updated class method")
production_slice(CLEAR lib/Epub/Epub/Section.cpp
  "bool Section::clearCache" "bool Section::createSectionFile")
production_slice(COUNT lib/Epub/Epub/Section.cpp
  "std::optional<uint16_t> Section::getCachedPageCount" "std::optional<uint16_t> Section::getPageForAnchor")
file(READ "${REPO_ROOT}/lib/Epub/Epub/Section.cpp" SECTION_SOURCE)
string(FIND "${SECTION_SOURCE}" "std::optional<uint16_t> Section::getPageForVisibleTextOffset" VISIBLE_START)
if(VISIBLE_START LESS 0)
  message(FATAL_ERROR "Missing production visible-offset reader")
endif()
string(SUBSTRING "${SECTION_SOURCE}" ${VISIBLE_START} -1 VISIBLE)
production_slice(COMMIT lib/Epub/Epub/Section.cpp
  "bool Section::commitBuildFile" "bool Section::finalizeBuild")
configure_file(SectionCacheContractTest.cpp.in SectionCacheContractTest.cpp @ONLY)
add_executable(SectionCacheContractTest "${CMAKE_CURRENT_BINARY_DIR}/SectionCacheContractTest.cpp")
target_include_directories(SectionCacheContractTest PRIVATE
  ${REPO_ROOT}/test/chapter_html_slim_parser/stubs ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Serialization)
target_link_libraries(SectionCacheContractTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(SectionCacheContractTest)
