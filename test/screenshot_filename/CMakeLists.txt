add_executable(ScreenshotFilenameTest ScreenshotFilenameTest.cpp)

target_link_libraries(ScreenshotFilenameTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ScreenshotFilenameTest)
