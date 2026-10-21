#pragma once
#include <string>
struct FsHelpers {
  static bool hasTxtExtension(const std::string& p) { return p.ends_with(".txt"); }
  static bool hasBmpExtension(const std::string& p) { return p.ends_with(".bmp"); }
  static bool hasJpgExtension(const std::string& p) { return p.ends_with(".jpg") || p.ends_with(".jpeg"); }
};
