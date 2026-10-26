#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace test_hal {
struct Entry {
  std::string name;
  bool directory;
};

inline std::vector<Entry> entries;

inline void setEntries(std::vector<Entry> newEntries) { entries = std::move(newEntries); }
}  // namespace test_hal

class HalFile {
 public:
  static HalFile directory() {
    HalFile file;
    file.open_ = true;
    file.isDirectory_ = true;
    return file;
  }

  explicit operator bool() const { return open_; }
  bool isDirectory() const { return isDirectory_ ? true : entry_.directory; }
  void rewindDirectory() { nextEntry_ = 0; }

  HalFile openNextFile() {
    if (!isDirectory_ || nextEntry_ >= test_hal::entries.size()) return {};
    return HalFile(test_hal::entries[nextEntry_++]);
  }

  size_t getName(char* name, const size_t length) {
    if (length == 0) return 0;
    const int copied = std::snprintf(name, length, "%s", entry_.name.c_str());
    return copied < 0 ? 0 : static_cast<size_t>(copied);
  }

  bool close() {
    open_ = false;
    return true;
  }

 private:
  explicit HalFile(test_hal::Entry entry) : entry_(std::move(entry)), open_(true) {}
  HalFile() = default;

  test_hal::Entry entry_{};
  size_t nextEntry_ = 0;
  bool open_ = false;
  bool isDirectory_ = false;
};

class HalStorage {
 public:
  HalFile open(const char*) { return HalFile::directory(); }
};

inline HalStorage Storage;