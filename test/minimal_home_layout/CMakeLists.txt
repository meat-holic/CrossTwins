add_executable(MinimalHomeLayoutTest MinimalHomeLayoutTest.cpp)

target_link_libraries(MinimalHomeLayoutTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(MinimalHomeLayoutTest)

add_executable(MinimalHomeSettingsJsonTest MinimalHomeSettingsJsonTest.cpp)
target_include_directories(MinimalHomeSettingsJsonTest PRIVATE ${REPO_ROOT})
target_link_libraries(MinimalHomeSettingsJsonTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  ArduinoJson
)
gtest_discover_tests(MinimalHomeSettingsJsonTest)
