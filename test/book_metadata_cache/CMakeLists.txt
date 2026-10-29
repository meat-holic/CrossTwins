add_executable(BookMetadataCacheTest
  BookMetadataCacheTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/BookMetadataCache.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(BookMetadataCacheTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/test/chapter_html_slim_parser/stubs
)

target_link_libraries(BookMetadataCacheTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

# This test replaces global new/delete with a counting allocator. Keep those
# hooks and the C++ runtime in one image on MinGW-family Windows hosts.
if(WIN32 AND MINGW)
  target_link_options(BookMetadataCacheTest PRIVATE -static)
endif()

gtest_discover_tests(BookMetadataCacheTest)
