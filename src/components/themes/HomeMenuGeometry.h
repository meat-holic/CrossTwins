#pragma once

#include <cstdint>

namespace home_menu {

enum class Alignment : uint8_t { LEFT = 0, CENTER = 1, RIGHT = 2 };
enum class Appearance : uint8_t { ROUNDED_CARDS, TEXT_CHIPS };

inline int alignedRowX(const int menuX, const int menuWidth, const int sidePadding, const int rowWidth,
                       const Alignment alignment) {
  switch (alignment) {
    case Alignment::LEFT:
      return menuX + sidePadding;
    case Alignment::CENTER:
      return menuX + (menuWidth - rowWidth) / 2;
    case Alignment::RIGHT:
      return menuX + menuWidth - sidePadding - rowWidth;
  }
  return menuX + sidePadding;
}

inline Alignment fromSetting(const uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(Alignment::CENTER):
      return Alignment::CENTER;
    case static_cast<uint8_t>(Alignment::RIGHT):
      return Alignment::RIGHT;
    case static_cast<uint8_t>(Alignment::LEFT):
    default:
      return Alignment::LEFT;
  }
}

}  // namespace home_menu
