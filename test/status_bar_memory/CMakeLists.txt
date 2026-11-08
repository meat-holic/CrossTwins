add_executable(StatusBarMemoryTitleTest StatusBarMemoryTitleTest.cpp)

target_include_directories(StatusBarMemoryTitleTest PRIVATE
  ${REPO_ROOT}/lib/Memory
)

target_link_libraries(StatusBarMemoryTitleTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(StatusBarMemoryTitleTest)
