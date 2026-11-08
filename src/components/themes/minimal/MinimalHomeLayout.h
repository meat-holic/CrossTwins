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
  float cropX;
  float cropY;
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

  int recentHeight = std::min(std::max(0, desiredRecentHeight), std::max(0, availableHeight - minimumMenuHeight - gap));
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

inline ImagePlacement centerCropImage(const Rect target, const int sourceWidth, const int sourceHeight) {
  if (target.width <= 0 || target.height <= 0 || sourceWidth <= 0 || sourceHeight <= 0) {
    return {target.x, target.y, 0, 0, 0.0f, 0.0f};
  }

  const float sourceAspect = static_cast<float>(sourceWidth) / sourceHeight;
  const float targetAspect = static_cast<float>(target.width) / target.height;
  float cropX = 0.0f;
  float cropY = 0.0f;
  if (sourceAspect > targetAspect) {
    cropX = 1.0f - targetAspect / sourceAspect;
  } else if (sourceAspect < targetAspect) {
    cropY = 1.0f - sourceAspect / targetAspect;
  }

  // Match GfxRenderer::drawBitmap's symmetric crop rounding and no-upscale fit.
  const int cropPixelsX = static_cast<int>(std::floor(sourceWidth * cropX / 2.0f));
  const int cropPixelsY = static_cast<int>(std::floor(sourceHeight * cropY / 2.0f));
  const float croppedWidth = (1.0f - cropX) * sourceWidth;
  const float croppedHeight = (1.0f - cropY) * sourceHeight;
  const float scale = std::min(
      {1.0f, static_cast<float>(target.width) / croppedWidth, static_cast<float>(target.height) / croppedHeight});
  const int sourcePixelsWidth = sourceWidth - cropPixelsX * 2;
  const int sourcePixelsHeight = sourceHeight - cropPixelsY * 2;
  const bool scaled = scale < 1.0f;
  const int width = scaled ? static_cast<int>(std::floor((sourcePixelsWidth - 1) * scale)) + 1 : sourcePixelsWidth;
  const int height = scaled ? static_cast<int>(std::floor((sourcePixelsHeight - 1) * scale)) + 1 : sourcePixelsHeight;
  return {target.x + (target.width - width) / 2, target.y + (target.height - height) / 2, width, height, cropX, cropY};
}

}  // namespace minimal_home
