#pragma once
#include <EpdFont.h>
class EpdFontFamily {
  const EpdFont* regular_;

 public:
  EpdFontFamily(const EpdFont* r, const EpdFont* = nullptr, const EpdFont* = nullptr, const EpdFont* = nullptr)
      : regular_(r) {}
  const EpdFontData* getData() const { return regular_->data; }
};
