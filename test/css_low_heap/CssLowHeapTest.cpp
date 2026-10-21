#include <Arduino.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "CssParser.h"

namespace {
bool countAllocations = false;
size_t allocations = 0;
}  // namespace

void* operator new(const size_t size) {
  if (countAllocations) ++allocations;
  if (void* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](size_t size) { return operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

TEST(CssLowHeap, LoadedCascadeRemainsAvailableWithoutLookupAllocations) {
  CssParser parser("");
#ifdef _WIN32
  const auto pid = _getpid();
#else
  const auto pid = getpid();
#endif
  const auto path = std::filesystem::temp_directory_path() /
                    ("ct-css-lookup-" + std::to_string(pid) + "-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    std::ofstream output(path);
    output << "p { text-align: center; } .note { font-weight: bold; } p.note { text-indent: 2em; }";
  }
  HalFile source;
  ASSERT_TRUE(source.open(path.string().c_str(), "rb"));
  const auto loaded = parser.loadFromStream(source);
  source.close();
  std::filesystem::remove(path);
  ASSERT_EQ(loaded, CssParser::ParseResult::Complete);
  const std::string classes = std::string(160, 'x') + " note";
  for (const uint32_t heap : {0u, 1024u, 48u * 1024u, UINT32_MAX}) {
    ESP.freeHeap = heap;
    allocations = 0;
    countAllocations = true;
    const auto style = parser.resolveStyle("P", classes);
    countAllocations = false;
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(style.textAlign, CssTextAlign::Center);
    EXPECT_EQ(style.fontWeight, CssFontWeight::Bold);
    EXPECT_TRUE(style.hasTextIndent());
    EXPECT_FLOAT_EQ(style.textIndent.value, 2.0f);
  }
  ESP.freeHeap = UINT32_MAX;
}
