#pragma once

#include <SdCardFontRegistry.h>

#include <cstring>
#include <vector>

namespace uiFontSelection {

inline bool shouldDeferLegacyMigration(const bool hasSavedUiPath, const bool sdUiEnabled,
                                       const char* readerFamilyName) {
  return !hasSavedUiPath && sdUiEnabled && readerFamilyName && *readerFamilyName != '\0';
}

inline uint16_t indexForPath(const std::vector<std::string>& options, const size_t selectableCount,
                             const uint16_t unavailableIndex, const char* selectedPath) {
  if (!selectedPath || *selectedPath == '\0') return 0;
  for (size_t i = 1; i <= selectableCount && i < options.size() && i <= UINT16_MAX; ++i) {
    if (options[i] == selectedPath) return static_cast<uint16_t>(i);
  }
  return unavailableIndex;
}

inline bool setPathForIndex(char* destination, const size_t destinationSize, const std::vector<std::string>& options,
                            const size_t selectableCount, const uint16_t unavailableIndex, const uint16_t index) {
  if (!destination || destinationSize == 0) return false;
  if (index == unavailableIndex && unavailableIndex > selectableCount) return true;
  if (index != 0 && (index > selectableCount || index >= options.size())) return false;
  const char* path = index == 0 ? "" : options[index].c_str();
  const size_t length = strlen(path);
  if (length >= destinationSize) return false;
  memcpy(destination, path, length + 1);
  return true;
}

// UI geometry uses one font face for common controls. Permit CPFonts across a
// broad line-height range, but reject obviously mismatched or corrupt metrics
// after loading. The picker itself still lists every discovered .cpfont.
inline bool hasSupportedMetrics(const int rasterSizePx, const int advanceY, const int ascender, const int descender) {
  constexpr int MIN_RASTER_SIZE_PX = 8;
  constexpr int MAX_RASTER_SIZE_PX = 40;
  constexpr int MIN_ADVANCE_Y = 8;
  constexpr int MAX_ADVANCE_Y = 40;
  constexpr int MAX_ASCENDER = 40;
  constexpr int MIN_DESCENDER = -20;
  constexpr int MAX_TOTAL_HEIGHT = 48;

  return rasterSizePx >= MIN_RASTER_SIZE_PX && rasterSizePx <= MAX_RASTER_SIZE_PX && advanceY >= MIN_ADVANCE_Y &&
         advanceY <= MAX_ADVANCE_Y && ascender > 0 && ascender <= MAX_ASCENDER && descender >= MIN_DESCENDER &&
         descender <= 0 && ascender - descender <= MAX_TOTAL_HEIGHT && ascender <= advanceY + 12;
}

inline bool isSelectableCpfont(const SdCardFontFamilyInfo& family, const SdCardFontFileInfo& file) {
  // Vector TTF/OTF/TTC faces have a separate loader and are not .cpfont files.
  return !family.vector && file.pointSize != 0;
}

inline bool findCpfontByPath(const std::vector<SdCardFontFamilyInfo>& families, const char* wantedPath,
                             const SdCardFontFamilyInfo*& familyOut, const SdCardFontFileInfo*& fileOut) {
  familyOut = nullptr;
  fileOut = nullptr;
  if (!wantedPath || *wantedPath == '\0') return false;

  for (const auto& family : families) {
    for (const auto& file : family.files) {
      if (!isSelectableCpfont(family, file)) continue;
      char path[SdCardFontFamilyInfo::PATH_BUFFER_SIZE];
      if (family.filePath(file, path, sizeof(path)) && strcmp(path, wantedPath) == 0) {
        familyOut = &family;
        fileOut = &file;
        return true;
      }
    }
  }
  return false;
}

}  // namespace uiFontSelection
