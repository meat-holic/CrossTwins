production_slice(DOWNLOAD_BOOK src/activities/browser/OpdsBookBrowserActivity.cpp
  "void OpdsBookBrowserActivity::downloadBook(" "// Open the publication detail page:")
configure_file(OpdsCtContractTest.cpp.in OpdsCtContractTest.cpp @ONLY)
add_executable(OpdsCtContractTest ${CMAKE_CURRENT_BINARY_DIR}/OpdsCtContractTest.cpp
  ${REPO_ROOT}/src/util/UrlUtils.cpp)
target_include_directories(OpdsCtContractTest PRIVATE ${REPO_ROOT}/src/util
  ${REPO_ROOT}/freeink-sdk/libs/network/Opds/include)
target_link_libraries(OpdsCtContractTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(OpdsCtContractTest)
