# Exercise the production handlers and metadata with the same JSON library as firmware.
find_path(SETTINGS_JSON_INCLUDE ArduinoJson.h
  PATHS "${REPO_ROOT}/.pio/libdeps/default/ArduinoJson/src")
if(NOT SETTINGS_JSON_INCLUDE)
  FetchContent_Declare(settings_json
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.2)
  FetchContent_MakeAvailable(settings_json)
  set(SETTINGS_JSON_INCLUDE "${settings_json_SOURCE_DIR}/src")
endif()
production_slice(SETTING_INFO src/activities/settings/SettingsActivity.h
  "enum class SettingType" "class SettingsActivity final")
file(READ "${REPO_ROOT}/src/SettingsList.cpp" metadata)
string(FIND "${metadata}" "// --- KOReader Sync" first)
string(FIND "${metadata}" "StrId::STR_SYNC_SERVER_URL" last)
string(SUBSTRING "${metadata}" 0 ${last} before_server)
string(FIND "${before_server}" "emit(" last REVERSE)
math(EXPR length "${last} - ${first}")
string(SUBSTRING "${metadata}" ${first} ${length} KO_METADATA)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REPO_ROOT}/src/SettingsList.cpp")
production_slice(GET_SETTINGS src/network/CrossPointWebServer.cpp
  "void CrossPointWebServer::handleGetSettings()" "void CrossPointWebServer::handlePostSettings()")
production_slice(POST_SETTINGS src/network/CrossPointWebServer.cpp
  "void CrossPointWebServer::handlePostSettings()" "// ---- OPDS Server API ----")
production_slice(AUTH_HEADERS lib/KOReaderSync/KOReaderSyncClient.cpp
  "void applyAuthHeaders(" "// Caller-configured headers")
production_slice(VALIDATE_SERVER lib/KOReaderSync/KOReaderCredentialStore.cpp
  "bool KOReaderCredentialStore::isValidServerConfiguration(" "bool KOReaderCredentialStore::saveServerConfiguration(")
configure_file(SettingsPasswordTest.cpp.in SettingsPasswordTest.cpp @ONLY)
add_executable(SettingsPasswordTest "${CMAKE_CURRENT_BINARY_DIR}/SettingsPasswordTest.cpp")
target_include_directories(SettingsPasswordTest PRIVATE "${SETTINGS_JSON_INCLUDE}")
target_link_libraries(SettingsPasswordTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(SettingsPasswordTest)
find_program(SETTINGS_NODE node)
if(SETTINGS_NODE)
  add_test(NAME SettingsPasswordBrowser
    COMMAND "${SETTINGS_NODE}" --test "${CMAKE_CURRENT_SOURCE_DIR}/SettingsPasswordBrowser.test.cjs")
else()
  message(WARNING "Node unavailable: settings password browser tests must be run separately")
endif()
