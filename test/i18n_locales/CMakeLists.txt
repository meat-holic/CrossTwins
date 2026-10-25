set(I18N_DIR "${REPO_ROOT}/lib/I18n")
set(I18N_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GLOB I18N_TRANSLATION_FILES CONFIGURE_DEPENDS "${I18N_DIR}/translations/*.yaml")

set(I18N_GENERATED_KEYS "${I18N_GENERATED_DIR}/I18nKeys.h")
set(I18N_GENERATED_STRINGS_H "${I18N_GENERATED_DIR}/I18nStrings.h")
set(I18N_GENERATED_STRINGS_CPP "${I18N_GENERATED_DIR}/I18nStrings.cpp")
add_custom_command(
  OUTPUT ${I18N_GENERATED_KEYS} ${I18N_GENERATED_STRINGS_H} ${I18N_GENERATED_STRINGS_CPP}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${I18N_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE} "${REPO_ROOT}/scripts/gen_i18n.py"
    "${I18N_DIR}/translations" "${I18N_GENERATED_DIR}"
    --src-dirs "${REPO_ROOT}/src" "${REPO_ROOT}/lib"
    --strip-unused --builtin-langs en,ko
  DEPENDS ${REPO_ROOT}/scripts/gen_i18n.py ${I18N_TRANSLATION_FILES}
  WORKING_DIRECTORY "${REPO_ROOT}"
  COMMENT "Generating English and Korean locale tables for host tests"
  VERBATIM
)
add_custom_target(generate_i18n_locale_tables
  DEPENDS ${I18N_GENERATED_KEYS} ${I18N_GENERATED_STRINGS_H} ${I18N_GENERATED_STRINGS_CPP}
)

add_executable(I18nLocaleFallbackTest
  I18nLocaleFallbackTest.cpp
  ${REPO_ROOT}/lib/I18n/I18n.cpp
  ${I18N_GENERATED_STRINGS_CPP}
)
add_dependencies(I18nLocaleFallbackTest generate_i18n_locale_tables)
target_include_directories(I18nLocaleFallbackTest BEFORE PRIVATE
  ${I18N_GENERATED_DIR}
  ${I18N_DIR}
  ${REPO_ROOT}/lib
)
target_link_libraries(I18nLocaleFallbackTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)
gtest_discover_tests(I18nLocaleFallbackTest)
