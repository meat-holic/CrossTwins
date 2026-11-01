add_executable(UiFontFallbackTest
  UiFontFallbackTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(UiFontFallbackTest PRIVATE
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(UiFontFallbackTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(UiFontFallbackTest)
