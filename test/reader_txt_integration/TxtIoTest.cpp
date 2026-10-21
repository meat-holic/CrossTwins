#include <JpegToBmpConverter.h>
#include <Txt.h>
#include <gtest/gtest.h>

class TxtIo : public testing::Test {
 protected:
  void SetUp() override {
    Storage = {};
    jpegSucceeds = true;
  }
  void TearDown() override {
    for (const auto& [path, count] : Storage.handles) EXPECT_EQ(count, 0) << path;
    EXPECT_FALSE(Storage.removedOpenFile);
  }
};

TEST_F(TxtIo, LoadAndDeepReadDoNotScanPrefixOrBuildIndex) {
  Storage.files["/book.txt"] = std::vector<uint8_t>(8 * 1024 * 1024, 'a');
  Txt txt("/book.txt", "/.crosspoint");
  ASSERT_TRUE(txt.load());
  EXPECT_EQ(Storage.reads, 0u);
  uint8_t bytes[32];
  ASSERT_TRUE(txt.readContent(bytes, txt.getFileSize() - sizeof(bytes), sizeof(bytes)));
  EXPECT_EQ(Storage.lastSeek, txt.getFileSize() - sizeof(bytes));
  EXPECT_EQ(Storage.reads, 1u);
  EXPECT_EQ(Storage.maxRead, sizeof(bytes));
  EXPECT_EQ(Storage.files.size(), 1u);
}
TEST_F(TxtIo, FailedLoadCanRetryAndRepeatedOpenClosesHandles) {
  Txt retry("/book.txt", "/.crosspoint");
  EXPECT_FALSE(retry.load());
  Storage.files["/book.txt"] = {'a', 'b'};
  EXPECT_TRUE(retry.load());
  for (int i = 0; i < 50; ++i) {
    Txt txt("/book.txt", "/.crosspoint");
    ASSERT_TRUE(txt.load());
    uint8_t byte;
    EXPECT_TRUE(txt.readContent(&byte, 1, 1));
  }
}
TEST_F(TxtIo, BoundsAndIoFailuresDoNotPublishSuccessfulRead) {
  Storage.files["/book.txt"] = {'a', 'b'};
  Txt txt("/book.txt", "/.crosspoint");
  uint8_t byte;
  EXPECT_FALSE(txt.readContent(&byte, 0, 1));
  ASSERT_TRUE(txt.load());
  EXPECT_FALSE(txt.readContent(&byte, SIZE_MAX, 1));
  EXPECT_FALSE(txt.readContent(&byte, 1, SIZE_MAX));
  EXPECT_FALSE(txt.readContent(nullptr, 0, 1));
  EXPECT_TRUE(txt.readContent(&byte, 2, 0));
  Storage.failSeek = true;
  EXPECT_FALSE(txt.readContent(&byte, 0, 1));
  Storage.failSeek = false;
  Storage.failRead = true;
  EXPECT_FALSE(txt.readContent(&byte, 0, 1));
  Storage.failRead = false;
  Storage.files["/book.txt"].clear();
  EXPECT_FALSE(txt.readContent(&byte, 0, 1));
}
TEST_F(TxtIo, BmpCopyIsBoundedAndLossless) {
  Storage.files["/books/book.bmp"] = std::vector<uint8_t>(1537, 42);
  Txt txt("/books/book.txt", "/.crosspoint");
  ASSERT_TRUE(txt.generateCoverBmp());
  EXPECT_EQ(Storage.files[txt.getCoverBmpPath()], Storage.files["/books/book.bmp"]);
  EXPECT_LE(Storage.maxRead, 128u);
}
TEST_F(TxtIo, BmpShortWriteClosesBeforeRemovingAndAllowsRetry) {
  Storage.files["/books/book.bmp"] = std::vector<uint8_t>(513, 42);
  Txt txt("/books/book.txt", "/.crosspoint");
  Storage.writeLimit = 3;
  EXPECT_FALSE(txt.generateCoverBmp());
  EXPECT_FALSE(Storage.exists(txt.getCoverBmpPath().c_str()));
  Storage.writeLimit = SIZE_MAX;
  EXPECT_TRUE(txt.generateCoverBmp());
}
TEST_F(TxtIo, BmpReadFailureIsNotConvertedToHugeUnsignedWrite) {
  Storage.files["/books/book.bmp"] = std::vector<uint8_t>(513, 42);
  Txt txt("/books/book.txt", "/.crosspoint");
  Storage.failRead = true;
  EXPECT_FALSE(txt.generateCoverBmp());
  EXPECT_FALSE(Storage.exists(txt.getCoverBmpPath().c_str()));
}
TEST_F(TxtIo, JpegFailureClosesBeforeRemovingOnlyCover) {
  Storage.files["/books/book.jpg"] = {1, 2, 3};
  Txt txt("/books/book.txt", "/.crosspoint");
  const auto progress = txt.getCachePath() + "/progress.bin";
  Storage.files[progress] = {4, 5, 6};
  jpegSucceeds = false;
  EXPECT_FALSE(txt.generateCoverBmp());
  EXPECT_FALSE(Storage.exists(txt.getCoverBmpPath().c_str()));
  EXPECT_EQ(Storage.files[progress], (std::vector<uint8_t>{4, 5, 6}));
}
