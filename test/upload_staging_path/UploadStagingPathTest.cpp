#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <unordered_set>

#include "util/UploadStagingPath.h"

TEST(UploadStagingPath, CreatesAnUploadSpecificSiblingWithoutTouchingFixedPartPath) {
  char selected[128] = {};
  const std::string destination = "/Books/book.epub";
  EXPECT_TRUE(upload_staging::selectPath(destination.c_str(), 0x12ab34cdu, [](const char*) { return false; }, selected,
                                         sizeof(selected)));
  EXPECT_STREQ(selected, "/Books/book.epub.upload-12ab34cd.part");
  EXPECT_STRNE(selected, (destination + ".part").c_str());
}

TEST(UploadStagingPath, SkipsOccupiedCandidates) {
  std::unordered_set<std::string> occupied = {"/Books/book.epub.upload-0000002a.part",
                                              "/Books/book.epub.upload-0000002b.part"};
  char selected[128] = {};
  EXPECT_TRUE(upload_staging::selectPath("/Books/book.epub", 0x2a,
                                         [&](const char* path) { return occupied.count(path) != 0; }, selected,
                                         sizeof(selected)));
  EXPECT_STREQ(selected, "/Books/book.epub.upload-0000002c.part");
  EXPECT_TRUE(occupied.count(selected) == 0);
}

TEST(UploadStagingPath, FailsClosedWhenAllCandidatesAreOccupied) {
  char selected[128] = "unchanged";
  EXPECT_FALSE(upload_staging::selectPath("/Books/book.epub", 0x0,
                                          [](const char*) { return true; }, selected, sizeof(selected)));
  EXPECT_STREQ(selected, "");
}
