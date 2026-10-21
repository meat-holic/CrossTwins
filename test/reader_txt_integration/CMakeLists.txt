add_executable(TxtIoTest TxtIoTest.cpp ${REPO_ROOT}/lib/Txt/Txt.cpp)
target_include_directories(TxtIoTest PRIVATE stubs ${REPO_ROOT}/lib/Txt)
target_link_libraries(TxtIoTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(TxtIoTest)

production_slice(READER_FACTORY src/activities/reader/ReaderActivity.cpp
  "std::unique_ptr<ReaderActivity> ReaderActivity::create" "void ReaderActivity::applyInitialOrientation()")
production_slice(EXTENSIONS lib/FsHelpers/FsHelpers.cpp "bool checkFileExtension(" "bool hasReflowableBookExtension(")
production_slice(MENU_ITEMS src/activities/reader/EpubReaderMenuActivity.cpp
  "void EpubReaderMenuActivity::buildMenuItems(" "void EpubReaderMenuActivity::toggleBluetooth()")
production_slice(TOGGLE_BLE src/activities/reader/EpubReaderMenuActivity.cpp
  "void EpubReaderMenuActivity::toggleBluetooth()" "void EpubReaderMenuActivity::closeCancelled()")
configure_file(ReaderPolicyTest.cpp.in ReaderPolicyTest.cpp @ONLY)
add_executable(ReaderPolicyTest ${CMAKE_CURRENT_BINARY_DIR}/ReaderPolicyTest.cpp)
target_link_libraries(ReaderPolicyTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ReaderPolicyTest)
