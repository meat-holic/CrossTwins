#pragma once

#include <algorithm>
#include <cmath>

namespace minimal_home {

struct Rect {
  int x;
  int y;
  int width;
  int height;
};

struct Layout {
  Rect background;
  Rect recent;
  Rect menu;
};

struct ImagePlacement {
  int x;
  int y;
  int width;
  int height;
};

inline Layout calculateLayout(const Rect safeArea, const int statusBottom, const int desiredRecentHeight,
                              const int menuItemCount, const int rowHeight, const int rowGap, const int sectionGap) {
  const int top = std::min(safeArea.y + safeArea.height, std::max(safeArea.y, statusBottom));
  const int bottom = std::max(safeArea.y, safeArea.y + safeArea.height);
  const int availableHeight = std::max(0, bottom - top);
  const int safeRowHeight = std::max(0, rowHeight);
  const int rowStep = safeRowHeight + std::max(0, rowGap);
  const int desiredMenuHeight = std::max(0, menuItemCount) * rowStep;
  const int minimumMenuHeight = menuItemCount > 0 ? std::min(safeRowHeight, availableHeight) : 0;
  const int gap = std::max(0, sectionGap);

  int recentHeight = std::min(std::max(0, desiredRecentHeight),
                              std::max(0, availableHeight - minimumMenuHeight - gap));
  int menuHeight = std::min(desiredMenuHeight, std::max(0, availableHeight - recentHeight - gap));
  if (menuItemCount > 0 && menuHeight < minimumMenuHeight) {
    menuHeight = minimumMenuHeight;
    recentHeight = std::min(recentHeight, std::max(0, availableHeight - menuHeight - gap));
  }

  const int menuY = bottom - menuHeight;
  const int recentHeightBeforeMenu = std::max(0, menuY - top - gap);
  recentHeight = std::min(recentHeight, recentHeightBeforeMenu);

  return {
      Rect{safeArea.x, top, std::max(0, safeArea.width), availableHeight},
      Rect{safeArea.x, top, std::max(0, safeArea.width), recentHeight},
      Rect{safeArea.x, menuY, std::max(0, safeArea.width), menuHeight},
  };
}

inline ImagePlacement fitImage(const Rect target, const int sourceWidth, const int sourceHeight) {
  if (target.width <= 0 || target.height <= 0 || sourceWidth <= 0 || sourceHeight <= 0) {
    return {target.x, target.y, 0, 0};
  }

  const float scale = std::min({1.0f, static_cast<float>(target.width) / sourceWidth,
                                static_cast<float>(target.height) / sourceHeight});
  const int width = std::max(1, static_cast<int>(std::floor(sourceWidth * scale)));
  const int height = std::max(1, static_cast<int>(std::floor(sourceHeight * scale)));
  return {target.x + (target.width - width) / 2, target.y + (target.height - height) / 2, width, height};
}

}  // namespace minimal_home
