#pragma once
#include <EpdFont.h>

#include <cstdint>
class SdCardFont {
 public:
  EpdFontData data{};
  EpdFont view{&data};
  static inline bool failLoad = false;
  ~SdCardFont();
  bool load(const char*) { return !failLoad; }
  uint32_t contentHash() const { return 0x12345678; }
  uint8_t styleCount() const { return 1; }
  EpdFont* getEpdFont(int) { return &view; }
};
