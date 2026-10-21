cmake_minimum_required(VERSION 3.16)
project(crosspoint_reader_tests C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_library(crosspoint_test_common INTERFACE)
target_include_directories(crosspoint_test_common INTERFACE
  ${REPO_ROOT}
  ${REPO_ROOT}/lib
)
target_compile_options(crosspoint_test_common INTERFACE
  -Wall
  -Wextra
  -pedantic
)

enable_testing()
include(GoogleTest)

# Production fragments used by the reviewed Slice 4-9 regression suites.
function(production_slice out file begin end)
  file(READ "${REPO_ROOT}/${file}" text)
  string(REPLACE "\r\n" "\n" text "${text}")
  string(FIND "${text}" "${begin}" start)
  if(start LESS 0)
    message(FATAL_ERROR "Missing production slice ${file}: ${begin}")
  endif()
  string(SUBSTRING "${text}" ${start} -1 rest)
  string(FIND "${rest}" "${end}" length)
  if(length LESS 0)
    message(FATAL_ERROR "Missing production slice end ${file}: ${end}")
  endif()
  string(SUBSTRING "${rest}" 0 ${length} result)
  set(${out} "${result}" PARENT_SCOPE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REPO_ROOT}/${file}")
endfunction()
find_package(EXPAT REQUIRED)
add_library(expat ALIAS EXPAT::EXPAT)
find_path(SETTINGS_JSON_INCLUDE ArduinoJson.h
  HINTS "${REPO_ROOT}/.pio/libdeps/default/ArduinoJson/src"
        "${REPO_ROOT}/.pio/libdeps/gh_release_rc/ArduinoJson/src")
if(NOT SETTINGS_JSON_INCLUDE)
  # CI's host job runs before firmware dependencies are installed. Fetch the
  # same pinned headers rather than depending on a local PlatformIO cache.
  FetchContent_Declare(arduinojson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.2
    SOURCE_SUBDIR no-build)
  FetchContent_MakeAvailable(arduinojson)
  set(SETTINGS_JSON_INCLUDE "${arduinojson_SOURCE_DIR}/src")
endif()
file(STRINGS "${SETTINGS_JSON_INCLUDE}/ArduinoJson/version.hpp" SETTINGS_JSON_VERSION
  REGEX "^#define ARDUINOJSON_VERSION \"7\\.4\\.2\"$")
if(NOT SETTINGS_JSON_VERSION)
  message(FATAL_ERROR "Settings tests require the firmware's ArduinoJson 7.4.2 headers")
endif()


add_subdirectory(streaming_json_parser)
add_subdirectory(release_json_parser)
add_subdirectory(differential_rounding)
add_subdirectory(hyphenation_eval)
add_subdirectory(utf8_compose)
add_subdirectory(opds_filename)
add_subdirectory(html_to_plain_text)
add_subdirectory(library_text)
add_subdirectory(library_format)
add_subdirectory(library_index_file)
add_subdirectory(library_builder)
add_subdirectory(minibidi_arabic)
add_subdirectory(combining_marks)
add_subdirectory(credential_integrity)
add_subdirectory(xml_parser_utils)
add_subdirectory(token_boundary)
add_subdirectory(koreader_xpath_resolver)
add_subdirectory(progress_comparison)
add_subdirectory(font_cache_manager)
add_subdirectory(css_parser)
add_subdirectory(chapter_html_slim_parser)
add_subdirectory(korean_line_breaking)
add_subdirectory(ligature_guard)
add_subdirectory(page_link)
add_subdirectory(content_opf_parser)
add_subdirectory(chapter_position)
add_subdirectory(home_button)
add_subdirectory(book_progress_snapshot)
add_subdirectory(txt_on_demand)

add_subdirectory(inflate_stream)
add_subdirectory(fs_helpers)

add_subdirectory(absolute_grayscale)

add_subdirectory(sd_card_font)
add_subdirectory(glyph_bitmap)
add_subdirectory(phase2a)
add_subdirectory(focus_reading)
add_subdirectory(phase2b)
add_subdirectory(reader_contention)
add_subdirectory(reader_remember)
add_subdirectory(epub_spine_chunks)

add_subdirectory(framebuffer_loan)

add_subdirectory(protected_paths)
add_subdirectory(settings_password)
add_subdirectory(section_soft_flush)
add_subdirectory(font_upload)
add_subdirectory(reader_toolbar)

add_subdirectory(ble_link_note_title)
add_subdirectory(ble_chapter_build_gate)
add_subdirectory(ble_remote_turn_gate)
add_subdirectory(${REPO_ROOT}/lib/BlePageTurner/test BlePageTurnerTests)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME BleLearning COMMAND ${CMAKE_COMMAND} -E env "CXX=${CMAKE_CXX_COMPILER}" ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ble_learning.py)
add_test(NAME RemoteButtonInputIntegration COMMAND ${CMAKE_COMMAND} -E env "CXX=${CMAKE_CXX_COMPILER}" ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_remote_button_input.py)
add_test(NAME BleBuildVersion COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ble_build_version.py)
add_test(NAME BleControllerLink COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ble_controller.py)
add_test(NAME BleBondedReconnect COMMAND ${CMAKE_COMMAND} -E env "CXX=${CMAKE_CXX_COMPILER}" ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ble_reconnect.py)
add_test(NAME I18nGenerationAndRuntime COMMAND ${CMAKE_COMMAND} -E env
  "CXX=${CMAKE_CXX_COMPILER}" "I18N_TEST_ARDUINOJSON_INCLUDE=${SETTINGS_JSON_INCLUDE}"
  ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_i18n.py)
add_test(NAME BleSettingsFailureBoundary COMMAND ${CMAKE_COMMAND} -E env
  "CXX=${CMAKE_CXX_COMPILER}" ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ble_settings_failure.py)

add_subdirectory(opds_feed_error)

add_subdirectory(em_dash_line_breaking)

add_subdirectory(book_metadata_cache)

add_subdirectory(button_press_navigation)

add_subdirectory(zip_file)

add_subdirectory(css_low_heap)

add_subdirectory(section_cache_contract)

add_subdirectory(sd_font_lifecycle)

add_subdirectory(settings_contract)

add_subdirectory(reader_txt_integration)

add_subdirectory(reader_ui_contract)

add_subdirectory(network_contract)
add_subdirectory(opds_ct_contract)
add_subdirectory(integration_remediation)
add_test(NAME ReleaseWorkflowContracts
  COMMAND ${Python3_EXECUTABLE} -m unittest discover -s scripts -p test_release_workflow_contracts.py)
set_tests_properties(ReleaseWorkflowContracts PROPERTIES WORKING_DIRECTORY "${REPO_ROOT}")

# Excluded-feature suites: txt_to_html, gif_support, clipping_*, reader_session, plugin_*.
