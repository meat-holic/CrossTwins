#pragma once

#include "components/themes/minimal/MinimalHomeLayout.h"
#include "components/themes/roundedraff/RoundedRaffTheme.h"

class GfxRenderer;

namespace MinimalMetrics {
constexpr ThemeMetrics values = [] {
  auto metrics = RoundedRaffMetrics::values;
  metrics.homeTopPadding = 62;
  metrics.homeCoverHeight = 0;
  metrics.homeCoverTileHeight = 56;
  metrics.homeMenuTopOffset = 14;
  metrics.homeRecentBooksCount = 1;
  metrics.homeContinueReadingInMenu = true;
  metrics.buttonHintsHeight = 40;
  return metrics;
}();
}  // namespace MinimalMetrics

struct MinimalHomeLayout {
  Rect background;
  Rect recent;
  Rect menu;
};

class MinimalTheme final : public RoundedRaffTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool backButton = true) const override;
  void drawHomeRecentBooks(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, const int* progress, size_t progressCount) const override;
  bool usesHomeCoverImages() const override { return false; }
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;

  static MinimalHomeLayout homeLayout(const GfxRenderer& renderer, int menuItemCount);
  static bool drawBackground(GfxRenderer& renderer, Rect contentArea, const char* imagePath);
  bool redrawMenuSelection(GfxRenderer& renderer, Rect rect, int buttonCount, int previousIndex, int selectedIndex,
                           const std::function<std::string(int index)>& buttonLabel) const;
};
