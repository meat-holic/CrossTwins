#pragma once
#include <EpdFontFamily.h>

#include <map>
class SdCardFont;
class TtfEpdFont;
class GfxRenderer {
 public:
  std::map<int, EpdFontFamily> fontMap;
  std::map<int, SdCardFont*> sdCardFonts_;
  std::map<int, unsigned> sdCardFontScales_;
  std::map<int, TtfEpdFont*> ttfFonts_;
  std::map<int, int> fallbackFontMap_;
  const auto& getFontMap() const { return fontMap; }
  void registerSdCardFont(int id, SdCardFont* font) { sdCardFonts_[id] = font; }
  void unregisterTtfFont(int id) { ttfFonts_.erase(id); }
  void insertFont(int, EpdFontFamily);
  void removeFont(int);
};
