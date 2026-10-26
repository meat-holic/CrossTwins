#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "FsHelpers.h"

namespace {

using namespace std::string_view_literals;

TEST(ReflowableBookExtension, IncludesTextFormatsAndExcludesImageBooks) {
  for (const auto path : {"/Books/book.epub"sv, "book.TXT"sv, "book.mD"sv}) {
    EXPECT_TRUE(FsHelpers::hasReflowableBookExtension(path));
  }
  for (const auto path : {"book.xtc"sv, "book.XTCH"sv, "book.bmp"sv, "book.epub.bak"sv, ""sv}) {
    EXPECT_FALSE(FsHelpers::hasReflowableBookExtension(path));
  }
}

TEST(IsSafePathComponent, AcceptsNamesWithRepeatedDots) {
  EXPECT_TRUE(FsHelpers::isSafePathComponent("volume..2.epub"sv));
  EXPECT_TRUE(FsHelpers::isSafePathComponent("notes...txt"sv));
  EXPECT_TRUE(FsHelpers::isSafePathComponent(".hidden"sv));
  EXPECT_TRUE(FsHelpers::isSafePathComponent("a.b"sv));
  EXPECT_TRUE(FsHelpers::isSafePathComponent("book.epub"sv));
}

TEST(IsSafePathComponent, RejectsEmptyAndExactDotComponents) {
  EXPECT_FALSE(FsHelpers::isSafePathComponent(""sv));
  EXPECT_FALSE(FsHelpers::isSafePathComponent("."sv));
  EXPECT_FALSE(FsHelpers::isSafePathComponent(".."sv));
}

TEST(IsSafePathComponent, RejectsPathSeparatorsAnywhereInTheComponent) {
  EXPECT_FALSE(FsHelpers::isSafePathComponent("a/b"sv));
  EXPECT_FALSE(FsHelpers::isSafePathComponent("a\\b"sv));
  EXPECT_FALSE(FsHelpers::isSafePathComponent("../x"sv));
  EXPECT_FALSE(FsHelpers::isSafePathComponent("x/.."sv));
}

TEST(NormalisePath, CollapsesParentReferenceWithinPath) {
  EXPECT_EQ(FsHelpers::normalisePath("/Books/../.crosspoint/x"), ".crosspoint/x");
}

TEST(NormalisePath, DropsLeadingParentReferencesPastRoot) { EXPECT_EQ(FsHelpers::normalisePath("/../../etc"), "etc"); }

// Sanitizes into a buffer of `size` bytes (at most 64, the size ScreenshotUtil uses).
std::string sanitize(const char* input, const size_t size = 64) {
  char out[64];
  FsHelpers::sanitizePathComponentForFat32(input, out, size);
  return out;
}

// Book titles from the EPUBs attached to #2103 and #2199.
constexpr char kTitle2103[] = "Богиня глюкозы. Нормализуйте уровень сахара в крови, чтобы изменить свою жизнь";
constexpr char kTitle2199[] = "Вглядываясь в солнце. Жизнь без страха смерти";

TEST(SanitizePathComponentForFat32, KeepsTitleThatFits) {
  EXPECT_EQ(sanitize("Эдем (полный перевод)"), "Эдем-(полный-перевод)");
}

// The readers copy the title into ScreenshotInfo::title (char[64]) with snprintf, which can
// end the copy partway through a Cyrillic letter.
TEST(SanitizePathComponentForFat32, DropsLetterCutOffByCaller) {
  char title[64];
  snprintf(title, sizeof(title), "%s", kTitle2103);
  EXPECT_EQ(sanitize(title), "Богиня-глюкозы.-Нормализуйте-уров");
  snprintf(title, sizeof(title), "%s", kTitle2199);
  EXPECT_EQ(sanitize(title), "Вглядываясь-в-солнце.-Жизнь-без-ст");
}

TEST(SanitizePathComponentForFat32, DoesNotSplitLetterAtBufferLimit) {
  // Each letter of "Жизнь" is 2 bytes. 7 bytes of room holds "Жиз" and half of "н".
  EXPECT_EQ(sanitize("Жизнь", 8), "Жиз");
  EXPECT_EQ(sanitize(kTitle2103), "Богиня-глюкозы.-Нормализуйте-уров");
}

void expectNaturalOrder(std::vector<std::string> names, const std::vector<std::string>& expected) {
  std::sort(names.begin(), names.end(), FsHelpers::naturalLess);
  EXPECT_EQ(names, expected);
}

TEST(NaturalLess, OrdersNumericComponentsBeforeFileExtensions) {
  expectNaturalOrder({"Book 10.epub", "Book 2.5.epub", "Book 1.5.epub", "Book 2.epub", "Book 1.epub"},
                     {"Book 1.epub", "Book 1.5.epub", "Book 2.epub", "Book 2.5.epub", "Book 10.epub"});
  expectNaturalOrder({"Book 6.epub", "Book 5.5.epub", "Book 5.epub"}, {"Book 5.epub", "Book 5.5.epub", "Book 6.epub"});
}

TEST(NaturalLess, OrdersDottedNumericComponents) {
  expectNaturalOrder({"1.10", "1.2.10", "1.2.3", "1.2", "1.02"}, {"1.2", "1.02", "1.2.3", "1.2.10", "1.10"});
  expectNaturalOrder({"v2.0", "v1.10", "v1.2"}, {"v1.2", "v1.10", "v2.0"});
}

TEST(NaturalLess, OrdersKoreanEnglishAndNumericTitles) {
  expectNaturalOrder({"10권", "2권", "1권"}, {"1권", "2권", "10권"});
  expectNaturalOrder({"한글 Book 10", "한글 Book 02", "한글 Book 2"}, {"한글 Book 2", "한글 Book 02", "한글 Book 10"});
  expectNaturalOrder({"Book 한글 10", "Book 한글 2"}, {"Book 한글 2", "Book 한글 10"});
}

TEST(NaturalLess, KeepsOrdinaryCaseInsensitiveStringOrdering) {
  expectNaturalOrder({"gamma", "Beta", "alpha"}, {"alpha", "Beta", "gamma"});
  EXPECT_TRUE(FsHelpers::naturalLess("Alpha", "beta"));
  EXPECT_FALSE(FsHelpers::naturalLess("zeta", "Alpha"));
}

TEST(NaturalLess, BreaksNumericTiesByOriginalSpelling) {
  expectNaturalOrder({"0001", "01", "001", "1"}, {"1", "01", "001", "0001"});
  expectNaturalOrder({"Book 10", "Book 02", "Book 1"}, {"Book 1", "Book 02", "Book 10"});
  EXPECT_NE(FsHelpers::naturalLess("Book 1.epub", "Book 01.epub"),
            FsHelpers::naturalLess("Book 01.epub", "Book 1.epub"));
}

TEST(NaturalEquivalent, IdentifiesNamesThatRemainAliasesForNextBook) {
  EXPECT_TRUE(FsHelpers::naturalEquivalent("Book 1.epub", "Book 01.epub"));
  EXPECT_TRUE(FsHelpers::naturalEquivalent("Book 1.epub", "Book 1.EPUB"));
  EXPECT_TRUE(FsHelpers::naturalEquivalent("Book 1.2.epub", "Book 1.02.epub"));
  EXPECT_TRUE(FsHelpers::naturalEquivalent("한글 2권.epub", "한글 02권.epub"));
  EXPECT_FALSE(FsHelpers::naturalEquivalent("Book 1.epub", "Book 1.5.epub"));
}

TEST(NaturalLess, HasStrictWeakOrderingOnRepresentativeNames) {
  const std::vector<std::string> names = {"",
                                          "0",
                                          "00",
                                          "1",
                                          "01",
                                          "001",
                                          "0001",
                                          "1.",
                                          "1..2",
                                          "1.0",
                                          "1.02",
                                          "1.2",
                                          "1.2.3",
                                          "1.2.10",
                                          "1.10",
                                          "1a",
                                          "a",
                                          "A",
                                          "a1",
                                          "a01",
                                          "Book 1.epub",
                                          "Book 01.epub",
                                          "Book 1.5.epub",
                                          "Book 2.epub",
                                          "Book 10.epub",
                                          "Bible 1.2",
                                          "Bible 1.10",
                                          "한글 1권",
                                          "한글 2권",
                                          "한글 10권",
                                          "한글 Book 2",
                                          "한글 Book 10"};
  const auto less = FsHelpers::naturalLess;
  for (const auto& a : names) {
    EXPECT_FALSE(less(a, a)) << a;
    for (const auto& b : names) {
      if (less(a, b)) EXPECT_FALSE(less(b, a)) << a << " / " << b;
      for (const auto& c : names) {
        if (less(a, b) && less(b, c)) EXPECT_TRUE(less(a, c)) << a << " / " << b << " / " << c;
        if (!less(a, b) && !less(b, a) && !less(b, c) && !less(c, b)) {
          EXPECT_FALSE(less(a, c)) << a << " / " << b << " / " << c;
          EXPECT_FALSE(less(c, a)) << a << " / " << b << " / " << c;
        }
      }
    }
  }
}
}  // namespace
