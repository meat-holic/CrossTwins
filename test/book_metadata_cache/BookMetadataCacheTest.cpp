#include <HalStorage.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "Epub/BookMetadataCache.h"
#include "ZipFile.h"

// Counting allocator: heap bytes allocated while `counting` is set and not yet freed.
namespace heap {
bool counting = false;
bool failNextArray = false;
bool failNextScalar = false;
size_t live = 0;

struct alignas(std::max_align_t) Header {
  size_t size;
  bool counted;
};

void* allocate(const size_t size) {
  auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + size));
  if (!header) return nullptr;
  header->size = size;
  header->counted = counting;
  if (counting) live += size;
  return header + 1;
}

void release(void* p) {
  if (!p) return;
  auto* header = static_cast<Header*>(p) - 1;
  if (header->counted) live -= header->size;
  std::free(header);
}
}  // namespace heap

void* operator new(const size_t size) {
  if (heap::failNextScalar) {
    heap::failNextScalar = false;
    throw std::bad_alloc();
  }
  void* p = heap::allocate(size);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](const size_t size) { return operator new(size); }
void* operator new(const size_t size, const std::nothrow_t&) noexcept { return heap::allocate(size); }
void* operator new[](const size_t size, const std::nothrow_t&) noexcept {
  if (heap::failNextArray) {
    heap::failNextArray = false;
    return nullptr;
  }
  return heap::allocate(size);
}
void operator delete(void* p) noexcept { heap::release(p); }
void operator delete[](void* p) noexcept { heap::release(p); }
void operator delete(void* p, size_t) noexcept { heap::release(p); }
void operator delete[](void* p, size_t) noexcept { heap::release(p); }

namespace {

std::string chapterHref(const int i) { return "OEBPS/Text/chapter" + std::to_string(i) + ".xhtml"; }
// Empty chapters, some across the window edge at item 32, give equal running totals.
uint32_t chapterBytes(const int i) {
  return (i % 37 == 5 || (i >= 30 && i < 35)) ? 0 : 1500 + static_cast<uint32_t>(i) * 7919 % 30000;
}

std::string cacheDir() {
  static const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
  const auto pid = _getpid();
#else
  const auto pid = getpid();
#endif
  return testing::TempDir() + "book_metadata_cache_" + std::to_string(pid) + "_" + std::to_string(stamp) + "_" +
         testing::UnitTest::GetInstance()->current_test_info()->name();
}

// Writes book.bin through the cache's own writer; returns every chapter's cumulative size.
std::vector<uint32_t> buildBook(const std::string& dir, const int spineCount) {
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  zipEntrySizes.clear();
  std::vector<uint32_t> cumulative;
  for (int i = 0; i < spineCount; i++) {
    zipEntrySizes[chapterHref(i)] = chapterBytes(i);
    cumulative.push_back((i > 0 ? cumulative.back() : 0) + chapterBytes(i));
  }
  BookMetadataCache cache(dir);
  EXPECT_TRUE(cache.beginWrite() && cache.beginContentOpfPass());
  for (int i = 0; i < spineCount; i++) cache.createSpineEntry(chapterHref(i));
  EXPECT_TRUE(cache.endContentOpfPass() && cache.beginTocPass());
  for (int i = 0; i < spineCount; i++) cache.createTocEntry("Chapter " + std::to_string(i), chapterHref(i), "", 1);
  EXPECT_TRUE(cache.endTocPass() && cache.endWrite());
  EXPECT_TRUE(cache.buildBookBin("book.epub", {}));
  EXPECT_TRUE(cache.cleanupTmpFiles());
  return cumulative;
}

}  // namespace

// A loaded book must not hold heap per chapter: 5,000 chapters may hold at most 1 KB more than 30.
TEST(BookMetadataCacheHeap, ResidentHeapDoesNotGrowWithSpineCount) {
  size_t resident[2] = {};
  for (const int spineCount : {30, 5000}) {
    const std::vector<uint32_t> cumulative = buildBook(cacheDir(), spineCount);
    BookMetadataCache cache(cacheDir());
    heap::live = 0;
    heap::counting = true;
    EXPECT_TRUE(cache.load());
    const size_t loaded = heap::live;
    for (int i = 0; i < spineCount; i++) EXPECT_EQ(cache.getCumulativeSize(i), cumulative[i]) << "spine " << i;
    EXPECT_EQ(heap::live, loaded) << spineCount << " chapters";
    heap::counting = false;
    resident[spineCount == 5000] = loaded;
  }
  EXPECT_LE(resident[1], resident[0] + 1024) << "30 chapters: " << resident[0] << " B";
}

// book.bin cut short inside a window: the window answers the total before it, never less.
TEST(BookMetadataCacheSizes, TruncatedWindowFallsBackToTheTotalBeforeIt) {
  const std::vector<uint32_t> cumulative = buildBook(cacheDir(), 100);
  const std::string path = cacheDir() + "/book.bin";
  std::ifstream in(path, std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const size_t href = bytes.find(chapterHref(64));
  ASSERT_NE(href, std::string::npos);
  BookMetadataCache cache(cacheDir());
  ASSERT_TRUE(cache.load());
  EXPECT_EQ(cache.getCumulativeSize(0), cumulative[0]);
  std::filesystem::resize_file(path, href + chapterHref(64).size());  // just before item 64's size
  EXPECT_EQ(cache.getCumulativeSize(63), cumulative[63]);
  EXPECT_EQ(cache.getCumulativeSize(64), cumulative[63]);
  // Failed position lookup is an error, not the last spine of a partially read window.
  for (int attempt = 0; attempt < 2; attempt++) {
    const int opens = Storage.readOpens;
    EXPECT_EQ(cache.getSpineIndexForSize(cumulative[70]), -1);
    EXPECT_EQ(Storage.readOpens - opens, 0) << "attempt " << attempt;
  }
}

// The lookup answers what a scan of every item answers, for every boundary, and reads at most one window.
TEST(BookMetadataCacheSizes, SpineIndexForSizeMatchesLinearScan) {
  for (const int spineCount : {100, 5000}) {
    const std::vector<uint32_t> cumulative = buildBook(cacheDir(), spineCount);
    BookMetadataCache scanned(cacheDir());
    BookMetadataCache cache(cacheDir());
    ASSERT_TRUE(scanned.load() && cache.load());
    std::vector<uint32_t> totals;
    for (int i = 0; i < spineCount; i++) totals.push_back(scanned.getCumulativeSize(i));
    const uint32_t bookSize = cumulative.back();
    std::vector<uint32_t> targets = {0, bookSize - 1, bookSize, bookSize + 1, 0};
    for (const uint32_t total : cumulative) targets.insert(targets.end(), {total - 1, total, total + 1});
    for (const uint32_t size : targets) {
      int expected = -1;
      for (int i = 0; i < spineCount && expected < 0; i++) {
        if (totals[i] >= size) expected = i;
      }
      const int opens = Storage.readOpens;
      EXPECT_EQ(cache.getSpineIndexForSize(size), expected) << spineCount << " items, size " << size;
      EXPECT_LE(Storage.readOpens - opens, 1) << spineCount << " items, size " << size;
    }
  }
}

TEST(BookMetadataCacheSizes, SparseIndexAllocationFailureCanRetryWithoutTouchingTheBook) {
  const auto cumulative = buildBook(cacheDir(), 100);
  BookMetadataCache cache(cacheDir());
  heap::failNextArray = true;
  EXPECT_FALSE(cache.load());
  EXPECT_FALSE(heap::failNextArray);
  EXPECT_FALSE(cache.isLoaded());
  EXPECT_EQ(cache.getCumulativeSize(0), 0u);
  EXPECT_EQ(cache.getSpineIndexForSize(1), -1);
  ASSERT_TRUE(cache.load());
  EXPECT_EQ(cache.getCumulativeSize(99), cumulative.back());
}

TEST(BookMetadataCacheSizes, TruncatedBookCacheFailsClosedWithoutRemovingIt) {
  buildBook(cacheDir(), 100);
  const auto path = cacheDir() + "/book.bin";
  std::ifstream input(path, std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  input.close();
  const size_t href = bytes.find(chapterHref(64));
  ASSERT_NE(href, std::string::npos);
  for (const size_t end : {size_t{0}, size_t{1}, size_t{8}, size_t{28}, href + 2}) {
    {
      std::ofstream output(path, std::ios::binary | std::ios::trunc);
      output.write(bytes.data(), end);
    }
    BookMetadataCache cache(cacheDir());
    EXPECT_FALSE(cache.load()) << end;
    EXPECT_FALSE(cache.isLoaded());
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(cache.getSpineIndexForSize(0), -1);
  }
}

TEST(BookMetadataCacheSizes, EmptyBookHasNoSparseWindowOrSpineMatch) {
  buildBook(cacheDir(), 0);
  BookMetadataCache cache(cacheDir());
  ASSERT_TRUE(cache.load());
  EXPECT_EQ(cache.getCumulativeSize(0), 0u);
  EXPECT_EQ(cache.getSpineIndexForSize(0), -1);
}

TEST(BookMetadataCacheSizes, CheckedHandleFailureAtLoadCanRetry) {
  buildBook(cacheDir(), 100);
  BookMetadataCache cache(cacheDir());
  HalFileTest::failNextCheckedOpen = true;
  EXPECT_FALSE(cache.load());
  EXPECT_FALSE(cache.isLoaded());
  EXPECT_FALSE(HalFileTest::failNextCheckedOpen);
  EXPECT_TRUE(cache.load());
}

TEST(BookMetadataCacheSizes, ColdRefillFailureDoesNotReturnAPositionOrPartialBounds) {
  const auto cumulative = buildBook(cacheDir(), 100);
  BookMetadataCache cache(cacheDir());
  ASSERT_TRUE(cache.load());
  const auto opens = Storage.readOpens;
  HalFileTest::readBytesLeft = 0;
  const int failedIndex = cache.getSpineIndexForSize(cumulative[70]);
  uint32_t start = 123, end = 456;
  const bool failedBounds = cache.tryGetSpineBounds(70, start, end);
  HalFileTest::readBytesLeft = SIZE_MAX;
  EXPECT_EQ(failedIndex, -1);
  EXPECT_FALSE(failedBounds);
  EXPECT_EQ(start, 123u);
  EXPECT_EQ(end, 456u);
  ASSERT_EQ(cache.getSpineIndexForSize(cumulative[70]), 70);
  ASSERT_TRUE(cache.tryGetSpineBounds(70, start, end));
  EXPECT_EQ(start, cumulative[69]);
  EXPECT_EQ(end, cumulative[70]);
  EXPECT_EQ(Storage.readOpens, opens) << "Window changes reuse the checked handle";
}

TEST(BookMetadataCacheSizes, ColdRefillNeedsNoScalarHeapAllocationOrNewHandle) {
  const auto cumulative = buildBook(cacheDir(), 100);
  BookMetadataCache cache(cacheDir());
  ASSERT_TRUE(cache.load());
  const auto opens = Storage.readOpens;
  uint32_t start = 0, end = 0;
  bool completed = false;
  heap::failNextScalar = true;
  try {
    completed = cache.tryGetSpineBounds(70, start, end);
  } catch (const std::bad_alloc&) {
  }
  const bool noAllocationRequested = heap::failNextScalar;
  heap::failNextScalar = false;
  EXPECT_TRUE(completed);
  EXPECT_TRUE(noAllocationRequested);
  EXPECT_EQ(end, cumulative[70]);
  EXPECT_EQ(Storage.readOpens, opens);
}

TEST(BookMetadataCacheLifecycle, FirstLoadAndEarlyFailuresNeverCloseANullHalHandle) {
  buildBook(cacheDir(), 100);
  HalFileTest::nullHandleCloses = 0;
  {
    BookMetadataCache cache(cacheDir());
    HalFileTest::failNextCheckedOpen = true;
    EXPECT_FALSE(cache.load());
    EXPECT_EQ(HalFileTest::nullHandleCloses, 0);
    EXPECT_TRUE(cache.load());
    EXPECT_TRUE(cache.load());
  }
  EXPECT_EQ(HalFileTest::nullHandleCloses, 0);
  std::filesystem::resize_file(cacheDir() + "/book.bin", 0);
  {
    BookMetadataCache cache(cacheDir());
    EXPECT_FALSE(cache.load());
  }
  EXPECT_EQ(HalFileTest::nullHandleCloses, 0);
}
