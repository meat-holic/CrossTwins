#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <FsHelpers.h>
#include <HalStorage.h>
#include "NextBookFinder.h"

namespace {

TEST(NextBookFinder, SortsCandidatesNaturallyAndSkipsEquivalentAliases) {
  test_hal::setEntries({{"Book 10.epub", false},
                        {"Book 2.epub", false},
                        {"Book 01.epub", false},
                        {"Book 1.EPUB", false},
                        {"Book 1.5.epub", false},
                        {"Book 3.epub", false},
                        {".Book 4.epub", false},
                        {"Folder 5.epub", true},
                        {"Unsupported 6.bin", false}});

  const auto books = NextBookFinder::findNextBooks("/Books/Book 1.epub", 5);
  EXPECT_EQ(books, (std::vector<std::string>{"Book 1.5.epub", "Book 2.epub", "Book 3.epub", "Book 10.epub"}));
}

TEST(NextBookFinder, KeepsOnlyTheFirstCandidatesAfterTheCurrentBook) {
  test_hal::setEntries({{"Book 10.epub", false},
                        {"Book 1.10.epub", false},
                        {"Book 1.2.epub", false},
                        {"Book 1.02.epub", false}});

  const auto books = NextBookFinder::findNextBooks("/Books/Book 1.epub", 2);
  EXPECT_EQ(books, (std::vector<std::string>{"Book 1.2.epub", "Book 1.02.epub"}));
}

TEST(NextBookFinder, SkipsDottedNumericLeadingZeroAliases) {
  test_hal::setEntries({{"Book 1.02.epub", false}, {"Book 1.10.epub", false}, {"Book 1.3.epub", false}});

  const auto books = NextBookFinder::findNextBooks("/Books/Book 1.2.epub", 5);
  EXPECT_EQ(books, (std::vector<std::string>{"Book 1.3.epub", "Book 1.10.epub"}));
}

}  // namespace
