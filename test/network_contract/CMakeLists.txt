file(READ "${REPO_ROOT}/src/network/OtaUpdater.cpp" OTA_SOURCE)
string(REGEX REPLACE "#include[^\n]*\n" "" OTA_SOURCE "${OTA_SOURCE}")
production_slice(BOARD_DECLARATION src/network/FirmwareBoardTag.h "class Scanner {" "}  // namespace board_tag")
production_slice(BOARD_SCANNER src/network/FirmwareBoardTag.cpp "void Scanner::feed(" "}  // namespace board_tag")
configure_file(OtaContractTest.cpp.in OtaContractTest.cpp @ONLY)
add_executable(OtaContractTest ${CMAKE_CURRENT_BINARY_DIR}/OtaContractTest.cpp
  ${REPO_ROOT}/lib/JsonParser/ReleaseJsonParser.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/src/StreamingJsonParser.cpp)
target_include_directories(OtaContractTest PRIVATE ${REPO_ROOT}/src ${REPO_ROOT}/lib/JsonParser
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/include)
target_link_libraries(OtaContractTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(OtaContractTest)
