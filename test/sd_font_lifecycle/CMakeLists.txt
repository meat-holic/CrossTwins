production_slice(RENDERER_METHODS lib/GfxRenderer/GfxRenderer.cpp
  "void GfxRenderer::insertFont" "int GfxRenderer::resolveTextFontId")
production_slice(REGISTRY_METHODS lib/EpdFont/SdCardFontRegistry.cpp
  "const SdCardFontFileInfo* SdCardFontFamilyInfo::findFile" "bool SdCardFontRegistry::parseFilename")
production_slice(REGISTRY_DISCOVERY lib/EpdFont/SdCardFontRegistry.cpp
  "bool SdCardFontRegistry::discover()" "const char* SdCardFontRegistry::findFamilyRoot")
production_slice(TTF_CLEANUP src/SdCardFontSystem.cpp
  "void SdCardFontSystem::freeTtfSources" "bool SdCardFontSystem::openTtfSource")
configure_file(FontLifecycleTest.cpp.in FontLifecycleTest.cpp @ONLY)
add_executable(FontLifecycleTest
  ${CMAKE_CURRENT_BINARY_DIR}/FontLifecycleTest.cpp
  ${REPO_ROOT}/lib/EpdFont/SdCardFontManager.cpp)
target_include_directories(FontLifecycleTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/EpdFont ${REPO_ROOT}/lib/Memory ${REPO_ROOT}/test/sd_card_font/stubs)
target_link_libraries(FontLifecycleTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(FontLifecycleTest)
