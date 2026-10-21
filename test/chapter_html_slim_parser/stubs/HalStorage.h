#pragma once

#include <climits>
#include <cstdint>
#include <cstdio>
#include <string>

namespace HalFileTest {
inline int nullHandleCloses = 0;
inline bool failNextWrite = false;
inline bool failNextClose = false;
inline bool failNextCheckedOpen = false;
inline size_t readBytesLeft = SIZE_MAX;
}  // namespace HalFileTest

constexpr int O_WRITE = 1;
constexpr int O_CREAT = 2;
constexpr int O_TRUNC = 4;

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(const char* path) { open(path, "wb"); }
  ~HalFile() {
    if (file_) close();
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  bool open(const char* path, const char* mode) {
    if (file_) close();
    hasImpl_ = true;
    file_ = std::fopen(path, mode);
    return file_ != nullptr;
  }
  int available() const { return file_ ? static_cast<int>(size() - position()) : 0; }
  size_t read(void* buffer, size_t count) {
    const size_t allowed = count < HalFileTest::readBytesLeft ? count : HalFileTest::readBytesLeft;
    const size_t bytes = file_ ? std::fread(buffer, 1, allowed, file_) : 0;
    HalFileTest::readBytesLeft -= bytes;
    return bytes;
  }
  size_t write(const void* buffer, size_t count) {
    if (HalFileTest::failNextWrite) {
      HalFileTest::failNextWrite = false;
      return 0;
    }
    const size_t allowed = count < writeBytesLeft_ ? count : writeBytesLeft_;
    const size_t written = file_ ? std::fwrite(buffer, 1, allowed, file_) : 0;
    writeBytesLeft_ -= written;
    return written;
  }
  size_t write(uint8_t byte) { return write(&byte, 1); }
  bool flush() { return file_ && std::fflush(file_) == 0; }
  bool seek(size_t pos) { return file_ && std::fseek(file_, static_cast<long>(pos), SEEK_SET) == 0; }
  bool seek64(uint64_t pos) { return pos <= LONG_MAX && seek(static_cast<size_t>(pos)); }
  bool seekCur(size_t offset) { return file_ && std::fseek(file_, static_cast<long>(offset), SEEK_CUR) == 0; }
  bool close() {
    // Real HalFile asserts when its wrapper was never allocated.
    if (!hasImpl_) ++HalFileTest::nullHandleCloses;
    if (!file_) return false;
    const bool ok = std::fclose(file_) == 0;
    file_ = nullptr;
    if (HalFileTest::failNextClose) {
      HalFileTest::failNextClose = false;
      return false;
    }
    return ok;
  }
  bool isOpen() const { return file_ != nullptr; }
  explicit operator bool() const { return isOpen(); }
  size_t position() const { return file_ ? static_cast<size_t>(std::ftell(file_)) : 0; }
  uint64_t fileSize64() const { return size(); }
  size_t size() const {
    if (!file_) return 0;
    const long offset = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    const long end = std::ftell(file_);
    std::fseek(file_, offset, SEEK_SET);
    return end > 0 ? static_cast<size_t>(end) : 0;
  }

  size_t write(const uint8_t* buffer, size_t count) { return write(static_cast<const void*>(buffer), count); }
  void limitWritesTo(size_t count) { writeBytesLeft_ = count; }

 private:
  bool hasImpl_ = false;
  std::FILE* file_ = nullptr;
  size_t writeBytesLeft_ = SIZE_MAX;
};

class HalStorage {
 public:
  enum class ReadOpenResult : uint8_t { Ok, IoFailure, ResourceFailure };
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  int readOpens = 0;
  std::string dictionaryTempPath;
  std::string resolvePath(const std::string& path) const {
    return path == "/.crosspoint/dicthtml.tmp" && !dictionaryTempPath.empty() ? dictionaryTempPath : path;
  }
  HalFile open(const char* path, int) { return HalFile(resolvePath(path).c_str()); }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    readOpens++;
    return file.open(resolvePath(path).c_str(), "rb");
  }
  ReadOpenResult openFileForReadChecked(const char* module, const char* path, HalFile& file) {
    if (HalFileTest::failNextCheckedOpen) {
      HalFileTest::failNextCheckedOpen = false;
      return ReadOpenResult::ResourceFailure;
    }
    return openFileForRead(module, path, file) ? ReadOpenResult::Ok : ReadOpenResult::IoFailure;
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) { return file.open(path.c_str(), "wb"); }
  bool exists(const char* path) const {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
  }
  bool remove(const std::string& path) { return std::remove(resolvePath(path).c_str()) == 0; }
  bool rename(const char* from, const char* to) { return std::rename(from, to) == 0; }
};

#define Storage HalStorage::getInstance()

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
