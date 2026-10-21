function(cache_contract_slice output path start finish)
  file(READ "${REPO_ROOT}/${path}" source)
  string(FIND "${source}" "${start}" first)
  string(FIND "${source}" "${finish}" last)
  if(first LESS 0 OR last LESS first)
    message(FATAL_ERROR "Missing cache position contract: ${path}")
  endif()
  math(EXPR length "${last}-${first}")
  string(SUBSTRING "${source}" ${first} ${length} body)
  set(${output} "${body}" PARENT_SCOPE)
endfunction()
cache_contract_slice(JUMP src/activities/reader/EpubReaderActivity.cpp
  "void EpubReaderActivity::jumpToPercent" "void EpubReaderActivity::applyProgressChange")
cache_contract_slice(APPLY src/activities/reader/EpubReaderActivity.cpp
  "void EpubReaderActivity::applyProgressChange" "void EpubReaderActivity::onReaderMenuConfirm")
cache_contract_slice(MAP lib/KOReaderSync/ProgressMapper.cpp
  "  if (xpathSpine >= 0 && xpathSpine < spineCount)" "  if (result.totalPages <= 0)")
cache_contract_slice(SYNC_GUARD src/activities/reader/KOReaderSyncActivity.cpp
  "    if (remotePosition.spineIndex < 0)" "    if (hasAlternateProgress)")
configure_file(PositionFailureTest.cpp.in PositionFailureTest.cpp @ONLY)
add_executable(BookMetadataCacheTest
  "${CMAKE_CURRENT_BINARY_DIR}/PositionFailureTest.cpp"
  ${REPO_ROOT}/lib/Epub/Epub/BookMetadataCache.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(BookMetadataCacheTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/test/chapter_html_slim_parser/stubs
)

target_link_libraries(BookMetadataCacheTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

if(MINGW)
  # Keep allocation/deallocation in the executable so the counting overrides
  # observe both ends, including STL temporaries created by the runtime.
  target_link_options(BookMetadataCacheTest PRIVATE -static)
endif()

gtest_discover_tests(BookMetadataCacheTest)
