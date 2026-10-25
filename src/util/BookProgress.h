#pragma once

#include <cstddef>
#include <string>

struct BookProgressSnapshot {
  static constexpr size_t CHAPTER_TITLE_CAPACITY = 257;

  int percent = -1;
  char chapterTitle[CHAPTER_TITLE_CAPACITY]{};
};

// Reads saved progress and an optional chapter title from existing cache files without parsing the EPUB.
BookProgressSnapshot loadBookProgressSnapshot(const std::string& path, bool includeChapter = false);

// Saved reading percentage, or -1 when unavailable. Never builds reading caches.
int loadBookProgress(const std::string& path);
