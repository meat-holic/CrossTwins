add_executable(FsHelpersTest
  FsHelpersTest.cpp
  ${REPO_ROOT}/lib/FsHelpers/FsHelpers.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(FsHelpersTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/FsHelpers
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(FsHelpersTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FsHelpersTest)

add_executable(NextBookFinderTest
  NextBookFinderTest.cpp
  ${REPO_ROOT}/src/util/NextBookFinder.cpp
  ${REPO_ROOT}/lib/FsHelpers/FsHelpers.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(NextBookFinderTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/next_book_stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/FsHelpers
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(NextBookFinderTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(NextBookFinderTest)
