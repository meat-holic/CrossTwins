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

# Compile the actual SDK translation unit against injected wolfSSL calls. No crypto or radio is mocked as hardware PASS.
file(READ "${REPO_ROOT}/freeink-sdk/libs/network/SecureNet/include/SecureClient.h" TLS_HEADER)
file(READ "${REPO_ROOT}/freeink-sdk/libs/network/SecureNet/src/SecureClient.cpp" TLS_SOURCE)
string(REGEX REPLACE "#include[^\n]*\n" "" TLS_HEADER "${TLS_HEADER}")
string(REGEX REPLACE "#include[^\n]*\n" "" TLS_SOURCE "${TLS_SOURCE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${REPO_ROOT}/freeink-sdk/libs/network/SecureNet/include/SecureClient.h"
  "${REPO_ROOT}/freeink-sdk/libs/network/SecureNet/src/SecureClient.cpp")
configure_file(SecureTlsContractTest.cpp.in SecureTlsContractTest.cpp @ONLY)
add_executable(SecureTlsContractTest ${CMAKE_CURRENT_BINARY_DIR}/SecureTlsContractTest.cpp)
target_link_libraries(SecureTlsContractTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(SecureTlsContractTest)

file(READ "${REPO_ROOT}/src/network/HttpDownloader.cpp" HTTP_SOURCE)
file(READ "${REPO_ROOT}/src/network/HttpDownloader.h" HTTP_HEADER)
string(REGEX REPLACE "#include[^\n]*\n" "" HTTP_SOURCE "${HTTP_SOURCE}")
string(REGEX REPLACE "#include[^\n]*\n" "" HTTP_HEADER "${HTTP_HEADER}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${REPO_ROOT}/src/network/HttpDownloader.cpp" "${REPO_ROOT}/src/network/HttpDownloader.h")
configure_file(HttpTlsContractTest.cpp.in HttpTlsContractTest.cpp @ONLY)
add_executable(HttpTlsContractTest ${CMAKE_CURRENT_BINARY_DIR}/HttpTlsContractTest.cpp)
target_include_directories(HttpTlsContractTest PRIVATE
 "${REPO_ROOT}/freeink-sdk/libs/network/SecureNet/test/host/stubs"
 "${REPO_ROOT}/freeink-sdk/libs/network/SecureNet/include")
target_link_libraries(HttpTlsContractTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(HttpTlsContractTest)

add_executable(SecureTlsNoCertsTest ${CMAKE_CURRENT_BINARY_DIR}/SecureTlsContractTest.cpp)
target_compile_definitions(SecureTlsNoCertsTest PRIVATE NO_CERTS=1)
target_link_libraries(SecureTlsNoCertsTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(SecureTlsNoCertsTest)
