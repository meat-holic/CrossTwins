production_slice(FONT_HANDLERS src/network/CrossPointWebServer.cpp
  "void CrossPointWebServer::handleFontUploadData()" "void CrossPointWebServer::handleFontDelete()")
production_slice(FONT_STATE src/network/CrossPointWebServer.h
  "  struct FontUploadState {" "  // OPDS server handlers")
production_slice(FONT_VALIDATE src/FontInstaller.cpp
  "bool FontInstaller::isValidFamilyName(" "bool FontInstaller::validateCpfontFile(")
production_slice(FONT_PATH src/FontInstaller.cpp
  "void FontInstaller::buildFontPath(" "FontInstaller::Error FontInstaller::deleteFamily(")
file(READ "${REPO_ROOT}/src/network/NetworkFileCommit.h" FONT_COMMIT)
string(REGEX REPLACE "#include[^\n]*\n" "" FONT_COMMIT "${FONT_COMMIT}")
configure_file(FontUploadTest.cpp.in FontUploadTest.cpp @ONLY)
add_executable(FontUploadTest "${CMAKE_CURRENT_BINARY_DIR}/FontUploadTest.cpp")
target_link_libraries(FontUploadTest PRIVATE crosspoint_test_common GTest::gtest_main)
target_include_directories(FontUploadTest PRIVATE ${REPO_ROOT}/lib/Memory)
gtest_discover_tests(FontUploadTest)
