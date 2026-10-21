#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct FakeStorage;
class HalFile {
  FakeStorage* owner = nullptr;
  std::string path;
  size_t pos = 0;

 public:
  HalFile() = default;
  ~HalFile() { close(); }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  void open(FakeStorage* storage, const std::string& name);
  bool close();
  size_t size() const;
  int available() const;
  bool seek(size_t offset);
  int read(void* dst, size_t count);
  size_t write(const uint8_t* src, size_t count);
};
struct FakeStorage {
  enum class ReadOpenResult { Ok, Failed };
  std::map<std::string, std::vector<uint8_t>> files;
  std::map<std::string, int> handles;
  size_t reads = 0, maxRead = 0, seeks = 0, lastSeek = 0;
  size_t writeLimit = std::numeric_limits<size_t>::max();
  bool failOpen = false, failRead = false, failSeek = false, removedOpenFile = false;
  bool exists(const char* path) const { return files.contains(path); }
  bool mkdir(const char*) { return true; }
  bool removeDir(const char*) { return true; }
  bool remove(const char* path) {
    removedOpenFile |= handles[path] != 0;
    return files.erase(path) != 0;
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    if (failOpen || !exists(path.c_str())) return false;
    file.open(this, path);
    return true;
  }
  ReadOpenResult openFileForReadChecked(const char* tag, const char* path, HalFile& file) {
    return openFileForRead(tag, path, file) ? ReadOpenResult::Ok : ReadOpenResult::Failed;
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) {
    if (failOpen) return false;
    files[path].clear();
    file.open(this, path);
    return true;
  }
};
using HalStorage = FakeStorage;
inline FakeStorage Storage;
inline void HalFile::open(FakeStorage* storage, const std::string& name) {
  close();
  owner = storage;
  path = name;
  pos = 0;
  ++owner->handles[path];
}
inline bool HalFile::close() {
  if (owner) --owner->handles[path];
  owner = nullptr;
  return true;
}
inline size_t HalFile::size() const { return owner ? owner->files.at(path).size() : 0; }
inline int HalFile::available() const { return owner && pos < size(); }
inline bool HalFile::seek(size_t offset) {
  if (!owner || owner->failSeek || offset > size()) return false;
  ++owner->seeks;
  owner->lastSeek = offset;
  pos = offset;
  return true;
}
inline int HalFile::read(void* dst, size_t count) {
  ++owner->reads;
  owner->maxRead = std::max(owner->maxRead, count);
  if (owner->failRead) return -1;
  count = std::min(count, size() - pos);
  std::memcpy(dst, owner->files.at(path).data() + pos, count);
  pos += count;
  return static_cast<int>(count);
}
inline size_t HalFile::write(const uint8_t* src, size_t count) {
  count = std::min(count, owner->writeLimit);
  owner->files[path].insert(owner->files[path].end(), src, src + count);
  return count;
}
