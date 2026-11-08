#include "MinimalTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <string>

#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kRecentSidePadding = 20;
constexpr int kRecentBlockPaddingY = 8;
constexpr int kMenuSectionGap = 18;
constexpr int kHomeHintFont = UI_10_FONT_ID;
constexpr int kRecentTitleFont = UI_12_FONT_ID;
constexpr int kRecentAuthorFont = UI_10_FONT_ID;

Rect toThemeRect(const minimal_home::Rect rect) { return Rect{rect.x, rect.y, rect.width, rect.height}; }

home_menu::Alignment menuAlignment() { return home_menu::fromSetting(SETTINGS.minimalMenuAlignment); }
}  // namespace

void MinimalTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* /*title*/,
                              const char* /*subtitle*/, const bool /*backButton*/) const {
  // Keep the shared battery/RTC header, but move all recent-book text below it.
  BaseTheme::drawHeader(renderer, rect, nullptr, nullptr, false);
}

MinimalHomeLayout MinimalTheme::homeLayout(const GfxRenderer& renderer, const int menuItemCount) {
  const Rect safeArea = UITheme::getInstance().getScreenSafeArea(renderer, !gpio.hasTouch(), false);
  const int recentHeight = renderer.getLineHeight(kRecentTitleFont) + renderer.getLineHeight(kRecentAuthorFont) +
                           kRecentBlockPaddingY * 2;
  const int statusBottom = std::max(MinimalMetrics::values.homeTopPadding, safeArea.y);
  const auto layout = minimal_home::calculateLayout(
      minimal_home::Rect{safeArea.x, safeArea.y, safeArea.width, safeArea.height}, statusBottom, recentHeight, menuItemCount,
      UITheme::getInstance().getTheme().getMenuRowHeight(renderer), MinimalMetrics::values.menuSpacing,
      kMenuSectionGap);
  return {toThemeRect(layout.background), toThemeRect(layout.recent), toThemeRect(layout.menu)};
}

bool MinimalTheme::drawBackground(GfxRenderer& renderer, const Rect contentArea, const char* imagePath) {
  if (contentArea.width <= 0 || contentArea.height <= 0 || imagePath == nullptr || imagePath[0] == '\0') return true;

  const auto clearContent = [&renderer, contentArea] {
    renderer.fillRect(contentArea.x, contentArea.y, contentArea.width, contentArea.height, false);
    return false;
  };

  HalFile file;
  if (!Storage.openFileForRead("MINIMAL", imagePath, file) || !file) {
    LOG_DBG("MIN", "%s", tr(STR_BACKGROUND_IMAGE_LOAD_FAILED));
    return clearContent();
  }

  auto bitmap = makeUniqueNoThrow<Bitmap>(file);
  if (!bitmap) {
    LOG_ERR("MIN", "OOM: background BMP decoder");
    return clearContent();
  }

  const BmpReaderError headerError = bitmap->parseHeaders();
  if (headerError != BmpReaderError::Ok) {
    LOG_ERR("MIN", "%s: %s", tr(STR_BACKGROUND_IMAGE_INVALID), Bitmap::errorToString(headerError));
    return clearContent();
  }

  const auto placement =
      minimal_home::fitImage({contentArea.x, contentArea.y, contentArea.width, contentArea.height}, bitmap->getWidth(),
                             bitmap->getHeight());
  if (placement.width <= 0 || placement.height <= 0 ||
      !renderer.drawBitmap(*bitmap, placement.x, placement.y, placement.width, placement.height)) {
    LOG_ERR("MIN", "%s", tr(STR_BACKGROUND_IMAGE_LOAD_FAILED));
    return clearContent();
  }
  return true;
}

void MinimalTheme::drawHomeRecentBooks(GfxRenderer& renderer, const Rect rect,
                                       const std::vector<RecentBook>& recentBooks, const int selectorIndex,
                                       const int* /*progress*/, const size_t /*progressCount*/) const {
  if (rect.width <= 0 || rect.height <= 0) return;

  const bool hasBook = !recentBooks.empty();
  const bool selected = hasBook && selectorIndex == 0;
  const char* titleText = hasBook ? recentBooks.front().title.c_str() : tr(STR_NO_OPEN_BOOK);
  const char* authorText = hasBook ? recentBooks.front().author.c_str() : "";
  const int maxTextWidth = std::max(0, rect.width - kRecentSidePadding * 2 - 24);
  const std::string title = renderer.truncatedText(kRecentTitleFont, titleText, maxTextWidth);
  const std::string author =
      authorText[0] != '\0' ? renderer.truncatedText(kRecentAuthorFont, authorText, maxTextWidth) : std::string();
  const int titleWidth = renderer.getTextWidth(kRecentTitleFont, title.c_str());
  const int authorWidth = author.empty() ? 0 : renderer.getTextWidth(kRecentAuthorFont, author.c_str());
  const int titleHeight = renderer.getLineHeight(kRecentTitleFont);
  const int authorHeight = author.empty() ? 0 : renderer.getLineHeight(kRecentAuthorFont);
  const int blockHeight = std::min(rect.height, titleHeight + authorHeight + kRecentBlockPaddingY * 2);
  const int blockWidth = std::min(rect.width - kRecentSidePadding * 2,
                                  std::max(titleWidth, authorWidth) + kRecentSidePadding);
  const int blockX = rect.x + (rect.width - blockWidth) / 2;
  const int blockY = rect.y + (rect.height - blockHeight) / 2;
  renderer.fillRect(blockX, blockY, blockWidth, blockHeight, selected);

  int textY = blockY + kRecentBlockPaddingY;
  renderer.drawText(kRecentTitleFont, rect.x + (rect.width - titleWidth) / 2, textY, title.c_str(), !selected);
  if (!author.empty()) {
    textY += titleHeight;
    const int authorY = textY + (authorHeight - renderer.getLineHeight(kRecentAuthorFont)) / 2;
    renderer.drawText(kRecentAuthorFont, rect.x + (rect.width - authorWidth) / 2, authorY, author.c_str(), !selected);
  }
}

void MinimalTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int buttonCount,
                                  const int selectedIndex,
                                  const std::function<std::string(int index)>& buttonLabel,
                                  const std::function<UIIcon(int index)>& /*rowIcon*/) const {
  drawButtonMenuWithStyle(renderer, rect, buttonCount, selectedIndex, buttonLabel, menuAlignment(),
                          home_menu::Appearance::TEXT_CHIPS);
}

bool MinimalTheme::redrawMenuSelection(GfxRenderer& renderer, const Rect rect, const int buttonCount,
                                       const int previousIndex, const int selectedIndex,
                                       const std::function<std::string(int index)>& buttonLabel) const {
  if (previousIndex == selectedIndex) return true;
  const int rowStep = getMenuRowHeight(renderer) + MinimalMetrics::values.menuSpacing;
  const int pageItems = std::max(1, rect.height / rowStep);
  const int previousPage = std::max(0, previousIndex) / pageItems;
  const int selectedPage = std::max(0, selectedIndex) / pageItems;
  if (previousPage != selectedPage) return false;

  const int pageStartIndex = selectedPage * pageItems;
  if (previousIndex >= pageStartIndex && previousIndex < buttonCount) {
    const std::string label = buttonLabel(previousIndex);
    drawButtonMenuRowWithStyle(renderer, rect, previousIndex, pageStartIndex, false, label.c_str(), menuAlignment(),
                               home_menu::Appearance::TEXT_CHIPS);
  }
  if (selectedIndex >= pageStartIndex && selectedIndex < buttonCount) {
    const std::string label = buttonLabel(selectedIndex);
    drawButtonMenuRowWithStyle(renderer, rect, selectedIndex, pageStartIndex, true, label.c_str(), menuAlignment(),
                               home_menu::Appearance::TEXT_CHIPS);
  }
  return true;
}

void MinimalTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                   const char* btn4) const {
  if (gpio.hasTouch()) return;

  const Rect content = UITheme::getContentArea(renderer);
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int height = UITheme::getInstance().getMetrics().buttonHintsHeight;
  if (height <= 0 || content.width <= 0 || content.height <= 0) return;

  Rect footer{};
  int separatorX1 = 0;
  int separatorY1 = 0;
  int separatorX2 = 0;
  int separatorY2 = 0;
  const auto orientation = renderer.getOrientation();
  if (orientation == GfxRenderer::Orientation::Portrait) {
    footer = Rect(safe.x, safe.y + safe.height, safe.width,
                  std::max(0, content.y + content.height - safe.y - safe.height));
    separatorX1 = footer.x;
    separatorY1 = footer.y;
    separatorX2 = footer.x + footer.width - 1;
    separatorY2 = footer.y;
  } else if (orientation == GfxRenderer::Orientation::PortraitInverted) {
    footer = Rect(safe.x, content.y, safe.width, std::max(0, safe.y - content.y));
    separatorX1 = footer.x;
    separatorY1 = footer.y + footer.height - 1;
    separatorX2 = footer.x + footer.width - 1;
    separatorY2 = separatorY1;
  } else if (orientation == GfxRenderer::Orientation::LandscapeClockwise) {
    footer = Rect(content.x, safe.y, std::max(0, safe.x - content.x), safe.height);
    separatorX1 = footer.x + footer.width - 1;
    separatorY1 = footer.y;
    separatorX2 = separatorX1;
    separatorY2 = footer.y + footer.height - 1;
  } else {
    footer = Rect(safe.x + safe.width, safe.y, std::max(0, content.x + content.width - safe.x - safe.width),
                  safe.height);
    separatorX1 = footer.x;
    separatorY1 = footer.y;
    separatorX2 = separatorX1;
    separatorY2 = footer.y + footer.height - 1;
  }
  if (footer.width <= 0 || footer.height <= 0) return;
  renderer.drawLine(separatorX1, separatorY1, separatorX2, separatorY2, 1, true);

  const char* labels[] = {btn1, btn2, btn3, btn4};
  const bool vertical = footer.height > footer.width;
  const int cellExtent = vertical ? footer.height : footer.width;
  const int cellSize = cellExtent / 4;
  for (int i = 0; i < 4; ++i) {
    if (labels[i] == nullptr || labels[i][0] == '\0') continue;
    const int start = i * cellSize;
    const int extent = i == 3 ? cellExtent - start : cellSize;
    if (vertical) {
      const int lineHeight = renderer.getLineHeight(kHomeHintFont);
      const std::string label = renderer.truncatedText(kHomeHintFont, labels[i], std::max(0, footer.width - 4));
      const int textWidth = renderer.getTextWidth(kHomeHintFont, label.c_str());
      const int x = footer.x + (footer.width - textWidth) / 2;
      const int y = footer.y + start + (extent - lineHeight) / 2;
      renderer.drawText(kHomeHintFont, x, y, label.c_str());
    } else {
      const int x = footer.x + start;
      const int textY = footer.y + (footer.height - renderer.getLineHeight(kHomeHintFont)) / 2;
      drawHintLabel(renderer, kHomeHintFont, labels[i], x, extent, textY, renderer.getLineHeight(kHomeHintFont));
    }
  }
}
