#pragma once
#ifndef CROSSPOINT_VECTOR_FONTS
#define CROSSPOINT_VECTOR_FONTS 0
#endif
#if CROSSPOINT_VECTOR_FONTS
#include <cstdio>
class TtfEpdFont {
 public:
  int clearCalls = 0, releaseCalls = 0, buildCalls = 0, addCalls = 0;
  char lastText[32]{};
  void clearCache() { ++clearCalls; }
  void releaseResidentCaches() { ++releaseCalls; }
  bool build(const char* s) {
    ++buildCalls;
    std::snprintf(lastText, sizeof(lastText), "%s", s);
    return true;
  }
  bool addCoverage(const char* s) {
    ++addCalls;
    std::snprintf(lastText, sizeof(lastText), "%s", s);
    return true;
  }
};
#else
class TtfEpdFont;
#endif
