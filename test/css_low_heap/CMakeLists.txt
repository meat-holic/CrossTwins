add_executable(CssLowHeapTest CssLowHeapTest.cpp ${REPO_ROOT}/lib/Epub/Epub/css/CssParser.cpp)
target_include_directories(CssLowHeapTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${REPO_ROOT}/test/css_parser/stubs
  ${REPO_ROOT}/lib/Epub/Epub/css ${REPO_ROOT}/lib/Memory)
target_link_libraries(CssLowHeapTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(CssLowHeapTest)
