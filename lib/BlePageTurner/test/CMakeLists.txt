# Can configure this directory directly or use the firmware host suite. Both use
# the pinned SDK profile header; runtime tests replace the radio with Fakes.cpp.
# cmake -S lib/BlePageTurner/test -B build/bt && cmake --build build/bt && ctest --test-dir build/bt
# Offline builds can set -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<googletest>
# and -DFETCHCONTENT_SOURCE_DIR_ARDUINOJSON=<ArduinoJson>.
cmake_minimum_required(VERSION 3.16)
project(ble_page_turner_tests CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  enable_testing()
endif()

include(FetchContent)
if(NOT TARGET GTest::gtest_main)
  FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.17.0)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

get_filename_component(BLETURNER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(BLETURNER_SDK_ROOT "${BLETURNER_ROOT}/../../freeink-sdk" CACHE PATH "SDK checkout for isolated BLE tests")

add_library(bleturner_test_flags INTERFACE)
target_include_directories(bleturner_test_flags INTERFACE
  ${BLETURNER_ROOT}/include ${BLETURNER_ROOT}/src
  ${BLETURNER_SDK_ROOT}/libs/network/BleKeyboardHost/include
)
target_compile_options(bleturner_test_flags INTERFACE -Wall -Wextra -Werror)

# ArduinoJson at the version the firmware pins (platformio.ini: bblanchon/ArduinoJson @ 7.4.2). A
# local copy works too: only its src/ is used, never its own build.
FetchContent_Declare(arduinojson GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git GIT_TAG v7.4.2
                     SOURCE_SUBDIR no-build)
FetchContent_MakeAvailable(arduinojson)
add_library(bleturner_arduinojson INTERFACE)
target_include_directories(bleturner_arduinojson SYSTEM INTERFACE ${arduinojson_SOURCE_DIR}/src)

# The rules, pure.
add_executable(BlePageTurnerPolicyTest RadioVerdictTest.cpp RadioVerdictS3Test.cpp HeapRestartTest.cpp KeyBindingTest.cpp)
target_link_libraries(BlePageTurnerPolicyTest PRIVATE bleturner_test_flags GTest::gtest_main)
gtest_discover_tests(BlePageTurnerPolicyTest)

# The runtime against a fake radio and a fake host.
add_executable(BlePageTurnerRuntimeTest RuntimeTest.cpp TickTest.cpp ReconnectGateTest.cpp LinkNoteTest.cpp RequestConnectTest.cpp Fakes.cpp ${BLETURNER_ROOT}/src/Runtime.cpp)
target_compile_definitions(BlePageTurnerRuntimeTest PRIVATE BLETURNER_TESTING)
target_link_libraries(BlePageTurnerRuntimeTest PRIVATE bleturner_test_flags GTest::gtest_main)
gtest_discover_tests(BlePageTurnerRuntimeTest)

# A build without BLE.
add_executable(BlePageTurnerNoRadioTest NoRadioTest.cpp ${BLETURNER_ROOT}/src/Runtime.cpp ${BLETURNER_ROOT}/src/NoRadio.cpp)
target_compile_definitions(BlePageTurnerNoRadioTest PRIVATE BLETURNER_TESTING)
target_link_libraries(BlePageTurnerNoRadioTest PRIVATE bleturner_test_flags GTest::gtest_main)
gtest_discover_tests(BlePageTurnerNoRadioTest)

# The settings keys.
add_executable(BlePageTurnerConfigTest ConfigJsonTest.cpp)
target_link_libraries(BlePageTurnerConfigTest PRIVATE bleturner_test_flags bleturner_arduinojson GTest::gtest_main)
gtest_discover_tests(BlePageTurnerConfigTest)

# What the module may include.
add_executable(BlePageTurnerSourceContractTest SourceContractTest.cpp)
target_compile_definitions(BlePageTurnerSourceContractTest PRIVATE BLETURNER_ROOT_PATH="${BLETURNER_ROOT}")
target_link_libraries(BlePageTurnerSourceContractTest PRIVATE bleturner_test_flags GTest::gtest_main)
gtest_discover_tests(BlePageTurnerSourceContractTest)

# Upstream HomeStartTest is deferred: CT has not adopted Home startup or external NimBLE pools.
# Compile the actual selected policy for both MCUs and for a PSRAM flag without enabling it.
foreach(profile IN ITEMS C3 S3 S3_PSRAM_FLAG)
  add_executable(BleCapability${profile}Test CapabilityTest.cpp)
  target_link_libraries(BleCapability${profile}Test PRIVATE bleturner_test_flags GTest::gtest_main)
  if(profile STREQUAL "C3")
    target_compile_definitions(BleCapability${profile}Test PRIVATE CONFIG_IDF_TARGET_ESP32C3=1)
  else()
    target_compile_definitions(BleCapability${profile}Test PRIVATE CONFIG_IDF_TARGET_ESP32S3=1 TEST_S3=1)
  endif()
  if(profile STREQUAL "S3_PSRAM_FLAG")
    target_compile_definitions(BleCapability${profile}Test PRIVATE FREEINK_BLE_STACK_IN_PSRAM=1)
  endif()
  gtest_discover_tests(BleCapability${profile}Test TEST_PREFIX "${profile}.")
endforeach()

# The production adapter + complete adopted SDK BLE implementation, using the SDK's transport fakes.
set(BLE_SDK_HOST "${BLETURNER_SDK_ROOT}/libs/network/BleKeyboardHost")
add_executable(BleProductionAdapterTest ProductionAdapterTest.cpp
  ${BLETURNER_ROOT}/src/Runtime.cpp ${BLETURNER_ROOT}/src/SdkRadio.cpp
  ${BLE_SDK_HOST}/src/BleKeyboardHost.cpp ${BLE_SDK_HOST}/src/HidKeymap.cpp
  ${BLE_SDK_HOST}/test/host/fake_ble.cpp)
target_include_directories(BleProductionAdapterTest PRIVATE sdk_stubs
  ${BLE_SDK_HOST}/test/host/stubs ${BLE_SDK_HOST}/test/host ${BLE_SDK_HOST}/src)
target_compile_definitions(BleProductionAdapterTest PRIVATE BLETURNER_TESTING FREEINK_CAP_BLE_HID_HOST=1)
set_source_files_properties(${BLE_SDK_HOST}/src/BleKeyboardHost.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-variable")
target_link_libraries(BleProductionAdapterTest PRIVATE bleturner_test_flags GTest::gtest_main)
gtest_discover_tests(BleProductionAdapterTest)
