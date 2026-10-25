#include "BmpViewerActivity.h"

#include <Bitmap.h>
#include <Epub/converters/JpegToFramebufferConverter.h>
#include <Epub/converters/PngToFramebufferConverter.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char CUSTOM_SLEEP_ROOT_BMP[] = "/sleep.bmp";
constexpr char CUSTOM_SLEEP_TMP_BMP[] = "/sleep.bmp.tmp";
constexpr char JPEG_SLEEP_TMP_PXC[] = "/sleep.bmp.pxc.tmp";
constexpr char TRANSPARENT_SLEEP_ROOT_BMP[] = "/sleep-overlay.bmp";
constexpr char TRANSPARENT_SLEEP_ROOT_PNG[] = "/sleep-overlay.png";
constexpr size_t COPY_BUFFER_SIZE = 2048;
constexpr uint8_t GRAYSCALE_PALETTE[] = {0, 0, 0, 0, 85, 85, 85, 0, 170, 170, 170, 0, 255, 255, 255, 0};

RenderConfig fitImage(const ImageDimensions& dimensions, const GfxRenderer& renderer) {
  const float scale = std::min({static_cast<float>(renderer.getScreenWidth()) / dimensions.width,
                                static_cast<float>(renderer.getScreenHeight()) / dimensions.height, 1.0f});
  const int width = std::max(1, std::min(renderer.getScreenWidth(), static_cast<int>(dimensions.width * scale)));
  const int height = std::max(1, std::min(renderer.getScreenHeight(), static_cast<int>(dimensions.height * scale)));
  RenderConfig config{(renderer.getScreenWidth() - width) / 2, (renderer.getScreenHeight() - height) / 2, width,
                      height};
  config.useExactDimensions = true;
  return config;
}

bool overlaySleepMode() {
  const uint8_t mode = SETTINGS.sleepScreen;
  return mode == CrossPointSettings::SLEEP_SCREEN_MODE::TRANSPARENT_CUSTOM ||
         mode == CrossPointSettings::SLEEP_SCREEN_MODE::COVER_TRANSPARENT;
}
}  // namespace

BmpViewerActivity::BmpViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
    : Activity("BmpViewer", renderer, mappedInput), filePath(std::move(path)) {}

void BmpViewerActivity::loadSiblingImages() {
  siblingImages.clear();
  currentImageIndex = -1;

  if (filePath.empty()) return;

  std::string dirPath = FsHelpers::extractFolderPath(filePath);
  size_t lastSlash = filePath.find_last_of('/');
  std::string fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

  auto dir = Storage.open(dirPath.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  const auto name = makeUniqueNoThrow<char[]>(500);
  if (!name) {
    LOG_ERR("BMP", "OOM: sibling filename buffer");
    return;
  }
  size_t imageCount = 0;
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(name.get(), 500);
    if (name[0] != '.' && FsHelpers::hasImageExtension(std::string_view{name.get()})) ++imageCount;
  }
  dir.rewindDirectory();
  siblingImages.reserve(imageCount);

  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (!file.isDirectory()) {
      file.getName(name.get(), 500);
      if (name[0] != '.') {
        std::string fname(name.get());
        if (FsHelpers::hasImageExtension(fname)) {
          siblingImages.push_back(fname);
        }
      }
    }
    file.close();
  }
  dir.close();

  FsHelpers::sortFileList(siblingImages);

  const auto image = std::find(siblingImages.begin(), siblingImages.end(), fileName);
  if (image != siblingImages.end()) {
    currentImageIndex = static_cast<int>(image - siblingImages.begin());
  }
}

bool BmpViewerActivity::canSetSleepCover() const {
  return FsHelpers::hasBmpExtension(filePath) || FsHelpers::hasJpgExtension(filePath) ||
         (overlaySleepMode() && FsHelpers::hasPngExtension(filePath));
}

void BmpViewerActivity::drawSideHints() {
  if (mappedInput.hasTouch()) return;
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const int width = std::max(1, UITheme::getInstance().getMetrics().sideButtonHintsWidth);
  const int height = std::max(1, std::min(78, screenHeight / 6));
  const int gap = 5;
  const int right = std::max(0, screenWidth - width - 4);
  const int left = gpio.hasEdgeSideButtons() ? 4 : right;
  const int top = std::max(0, (screenHeight - 2 * height - gap) / 2);

  const auto drawHint = [this, width, height](const int x, const int y, const bool previous) {
    const bool grayscalePlane = renderer.getRenderMode() != GfxRenderer::BW && !renderer.grayPlanesAreAbsolute();
    renderer.fillRect(x, y, width, height, grayscalePlane);
    if (grayscalePlane) return;
    renderer.drawRect(x, y, width, height);
    const int centerX = x + width / 2;
    const int centerY = y + height / 2;
    for (int offset = 0; offset < 7; ++offset) {
      const int row = previous ? centerY - 6 + offset : centerY + 6 - offset;
      renderer.drawLine(centerX - offset, row, centerX + offset, row);
    }
  };
  drawHint(left, top, true);
  drawHint(gpio.hasEdgeSideButtons() ? right : left, gpio.hasEdgeSideButtons() ? top : top + height + gap, false);
}

void BmpViewerActivity::drawViewerHints() {
  if (slideshowActive) return;
  const auto labelForHardware = [this](const uint8_t hardware) -> const char* {
    if (hardware == SETTINGS.frontButtonBack) return tr(STR_BACK);
    if (hardware == SETTINGS.frontButtonConfirm) return canSetSleepCover() ? tr(STR_SET_SLEEP_COVER) : "";
    if (hardware == SETTINGS.frontButtonLeft) return siblingImages.size() > 1 ? tr(STR_SLIDESHOW_PLAY) : "";
    if (hardware == SETTINGS.frontButtonRight) return tr(STR_SETTINGS_TITLE);
    return "";
  };
  GUI.drawButtonHints(renderer, labelForHardware(HalGPIO::BTN_BACK), labelForHardware(HalGPIO::BTN_CONFIRM),
                      labelForHardware(HalGPIO::BTN_LEFT), labelForHardware(HalGPIO::BTN_RIGHT));
  drawSideHints();
}

bool BmpViewerActivity::renderImage() {
  const bool jpeg = FsHelpers::hasJpgExtension(filePath);
  JpegToFramebufferConverter jpegDecoder;
  PngToFramebufferConverter pngDecoder;
  ImageToFramebufferDecoder& decoder =
      jpeg ? static_cast<ImageToFramebufferDecoder&>(jpegDecoder) : static_cast<ImageToFramebufferDecoder&>(pngDecoder);
  ImageDimensions dimensions;
  if (!decoder.getDimensions(filePath, dimensions)) return false;
  if (dimensions.width <= 0 || dimensions.height <= 0) return false;

  const auto config = fitImage(dimensions, renderer);
  const auto drawImage = [&]() {
    if (!decoder.decodeToFramebuffer(filePath, renderer, config)) return false;
    drawViewerHints();
    return true;
  };

  if (!drawImage()) return false;
  if (!jpeg) {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return true;
  }

  const bool absolute = renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported();
  if (absolute && !renderer.displayGrayscaleBase(HalDisplay::GrayscaleMode::Absolute)) return false;
  if (!absolute) renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  bool ready = true;
  // ponytail: decode each plane; add a pixel cache if large-image latency needs it.
  for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
    renderer.clearScreen(absolute ? 0xFF : 0x00);
    renderer.setRenderMode(mode);
    if (!drawImage()) {
      ready = false;
      break;
    }
    if (mode == GfxRenderer::GRAYSCALE_LSB) {
      renderer.copyGrayscaleLsbBuffers();
    } else {
      renderer.copyGrayscaleMsbBuffers();
    }
  }
  if (ready) renderer.displayGrayBuffer();

  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  if (!drawImage()) ready = false;
  renderer.cleanupGrayscaleWithFrameBuffer();
  return ready;
}

void BmpViewerActivity::onEnter() {
  Activity::onEnter();

  if (siblingImages.empty() && !filePath.empty()) {
    loadSiblingImages();
  }

  slideshowActive = false;
  slideshowError = false;
  renderCurrentImage();
}

void BmpViewerActivity::render(RenderLock&&) {
  if (slideshowError) {
    showSlideshowError();
  } else {
    renderCurrentImage();
  }
}

bool BmpViewerActivity::renderCurrentImage() {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  Rect popupRect{};
  if (!slideshowActive) {
    popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    GUI.fillPopupProgress(renderer, popupRect, 20);  // Initial 20% progress
  }
  if (FsHelpers::hasPngExtension(filePath) || FsHelpers::hasJpgExtension(filePath)) {
    renderer.clearScreen();
    if (!renderImage()) {
      if (slideshowActive) return false;
      renderer.clearScreen();
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_FILE_OPEN_FAILED));
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return false;
    }
    return true;
  }

  HalFile file;
  // 1. Open the BMP file
  if (Storage.openFileForRead("BMP", filePath, file)) {
    Bitmap bitmap(file, true,
                  renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported() &&
                      display.getController() == HalDisplay::Controller::SSD1677);

    // 2. Parse headers to get dimensions
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      int x, y;

      if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
        float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
        const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

        if (ratio > screenRatio) {
          // Wider than screen
          x = 0;
          y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
        } else {
          // Taller than screen
          x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
          y = 0;
        }
      } else {
        // Center small images
        x = (pageWidth - bitmap.getWidth()) / 2;
        y = (pageHeight - bitmap.getHeight()) / 2;
      }

      // 4. Prepare Rendering
      if (!slideshowActive) GUI.fillPopupProgress(renderer, popupRect, 50);

      renderer.clearScreen();
      if (!renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0)) {
        if (slideshowActive) return false;
        renderer.clearScreen();
        renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_FILE_OPEN_FAILED));
        renderer.displayBuffer(HalDisplay::HALF_REFRESH);
        return false;
      }

      // Draw UI hints on the base layer
      drawViewerHints();
      if (bitmap.hasGreyscale()) {
        const bool absolute = renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported();
        if (absolute && !renderer.displayGrayscaleBase(HalDisplay::GrayscaleMode::Absolute)) return false;
        if (!absolute) renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
        bool planesReady = true;
        for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
          if (bitmap.rewindToData() != BmpReaderError::Ok) {
            LOG_ERR("BMP", "Failed to rewind bitmap for grayscale rendering");
            planesReady = false;
            break;
          }
          renderer.clearScreen(absolute ? 0xFF : 0x00);
          renderer.setRenderMode(mode);
          if (!renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0)) {
            planesReady = false;
            break;
          }
          drawViewerHints();
          if (mode == GfxRenderer::GRAYSCALE_LSB) {
            renderer.copyGrayscaleLsbBuffers();
          } else {
            renderer.copyGrayscaleMsbBuffers();
          }
        }
        if (planesReady) renderer.displayGrayBuffer();

        // Rebuild the BW framebuffer for popups and subsequent differential updates.
        renderer.setRenderMode(GfxRenderer::BW);
        renderer.clearScreen();
        if (bitmap.rewindToData() != BmpReaderError::Ok ||
            !renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0)) {
          LOG_ERR("BMP", "Failed to rewind bitmap to restore the BW framebuffer");
          renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_FILE_OPEN_FAILED));
          planesReady = false;
        }
        drawViewerHints();
        renderer.cleanupGrayscaleWithFrameBuffer();
        if (!planesReady && !slideshowActive) renderer.displayBuffer(HalDisplay::HALF_REFRESH);
        if (!planesReady) return false;
      } else {
        renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      }
      return true;

    } else {
      // Handle file parsing error
      if (slideshowActive) return false;
      renderer.clearScreen();
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_INVALID_BMP_FILE));
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return false;
    }

    file.close();
  } else {
    // Handle file open error
    if (slideshowActive) return false;
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_FILE_OPEN_FAILED));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return false;
  }
}

void BmpViewerActivity::onExit() {
  slideshowActive = false;
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

bool BmpViewerActivity::saveJpegSleepCover() {
  if (Storage.exists(JPEG_SLEEP_TMP_PXC) && !Storage.remove(JPEG_SLEEP_TMP_PXC)) {
    LOG_ERR("BMP", "Failed to remove previous sleep image pixels");
    return false;
  }
  bool keepBmpTemp = false;
  const ScopedCleanup cleanup{[&keepBmpTemp]() {
    Storage.remove(JPEG_SLEEP_TMP_PXC);
    if (!keepBmpTemp) Storage.remove(CUSTOM_SLEEP_TMP_BMP);
  }};

  JpegToFramebufferConverter decoder;
  ImageDimensions dimensions;
  if (!decoder.getDimensions(filePath, dimensions)) return false;
  auto config = fitImage(dimensions, renderer);
  if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP &&
      (dimensions.width > renderer.getScreenWidth() || dimensions.height > renderer.getScreenHeight())) {
    const float ratio = static_cast<float>(dimensions.width) / dimensions.height;
    const float screenRatio = static_cast<float>(renderer.getScreenWidth()) / renderer.getScreenHeight();
    config.x = config.y = 0;
    config.maxWidth = renderer.getScreenWidth();
    config.maxHeight = renderer.getScreenHeight();
    if (ratio > screenRatio) {
      config.sourceCropX = 1.0f - screenRatio / ratio;
    } else {
      config.sourceCropY = 1.0f - ratio / screenRatio;
    }
  }
  config.cachePath = JPEG_SLEEP_TMP_PXC;
  renderer.clearScreen();
  bool cacheWritten;
  if (!decoder.decodeToFramebuffer(filePath, renderer, config, cacheWritten) || !cacheWritten) {
    LOG_ERR("BMP", "Failed to decode sleep image pixels");
    return false;
  }

  HalFile input, output;
  if (!Storage.openFileForRead("BMP", JPEG_SLEEP_TMP_PXC, input)) return false;
  uint16_t width, height;
  if (input.read(&width, sizeof(width)) != sizeof(width) || input.read(&height, sizeof(height)) != sizeof(height) ||
      width != config.maxWidth || height != config.maxHeight) {
    LOG_ERR("BMP", "Invalid sleep image pixels");
    return false;
  }
  const size_t pixelRowBytes = (width + 3) / 4;
  const size_t bmpRowBytes = (width + 15) / 16 * 4;
  if (input.size() != 4 + pixelRowBytes * height) {
    LOG_ERR("BMP", "Incomplete sleep image pixels");
    return false;
  }
  auto row = makeUniqueNoThrow<uint8_t[]>(bmpRowBytes);
  if (!row) {
    LOG_ERR("BMP", "OOM: sleep image row");
    return false;
  }
  if (!Storage.openFileForWrite("BMP", CUSTOM_SLEEP_TMP_BMP, output)) return false;
  BmpHeader header;
  createBmpHeader(&header, width, height, BmpRowOrder::TopDown);
  header.infoHeader.biBitCount = 2;
  header.infoHeader.biClrUsed = header.infoHeader.biClrImportant = 4;
  header.infoHeader.biSizeImage = bmpRowBytes * height;
  header.fileHeader.bfOffBits = sizeof(header) - sizeof(header.colors) + sizeof(GRAYSCALE_PALETTE);
  header.fileHeader.bfSize = header.fileHeader.bfOffBits + header.infoHeader.biSizeImage;
  if (output.write(&header, sizeof(header) - sizeof(header.colors)) != sizeof(header) - sizeof(header.colors) ||
      output.write(GRAYSCALE_PALETTE, sizeof(GRAYSCALE_PALETTE)) != sizeof(GRAYSCALE_PALETTE)) {
    LOG_ERR("BMP", "Failed to write sleep image header");
    return false;
  }
  for (int y = 0; y < height; ++y) {
    if (input.read(row.get(), pixelRowBytes) != pixelRowBytes || output.write(row.get(), bmpRowBytes) != bmpRowBytes) {
      LOG_ERR("BMP", "Failed to write sleep image row %d", y);
      return false;
    }
  }
  if (!output.close()) {
    LOG_ERR("BMP", "Failed to close sleep image");
    return false;
  }
  if (!Storage.replaceFile(CUSTOM_SLEEP_TMP_BMP, CUSTOM_SLEEP_ROOT_BMP)) {
    // ponytail: FAT replacement is not atomic; keep the complete temp for recovery.
    keepBmpTemp = true;
    LOG_ERR("BMP", "Failed to publish sleep image");
    return false;
  }
  return true;
}

void BmpViewerActivity::doSetSleepCover() {
  GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));

  const bool jpeg = FsHelpers::hasJpgExtension(filePath);
  const bool transparentMode = !jpeg && overlaySleepMode();
  if (!canSetSleepCover()) return;

  const char* destination =
      transparentMode ? (FsHelpers::hasPngExtension(filePath) ? TRANSPARENT_SLEEP_ROOT_PNG : TRANSPARENT_SLEEP_ROOT_BMP)
                      : CUSTOM_SLEEP_ROOT_BMP;
  bool success = filePath == destination;

  if (jpeg) {
    success = saveJpegSleepCover();
  } else if (!success) {
    auto buffer = makeUniqueNoThrow<uint8_t[]>(COPY_BUFFER_SIZE);
    if (!buffer) {
      LOG_ERR("BMP", "OOM: sleep cover copy buffer");
    } else {
      // Copy beside the target and swap in only a complete image, so a failed
      // copy keeps the previous sleep cover instead of a truncated one.
      const std::string tmp = std::string(destination) + ".tmp";
      HalFile inFile, outFile;
      if (Storage.openFileForRead("BMP", filePath, inFile) && Storage.openFileForWrite("BMP", tmp, outFile)) {
        int bytesRead;
        success = true;
        while ((bytesRead = inFile.read(buffer.get(), COPY_BUFFER_SIZE)) > 0) {
          if (outFile.write(buffer.get(), static_cast<size_t>(bytesRead)) != static_cast<size_t>(bytesRead)) {
            success = false;
            break;
          }
        }
        if (bytesRead < 0) success = false;
        outFile.close();
        success = success && Storage.replaceFile(tmp.c_str(), destination);
        if (!success) Storage.remove(tmp.c_str());
      }
    }
  }

  if (success) {
    if (!transparentMode) SETTINGS.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM;
    SETTINGS.saveToFile();
    GUI.drawPopup(renderer, tr(STR_DONE));
  } else {
    GUI.drawPopup(renderer, tr(STR_FAILED_LOWER));
  }

  delay(1000);
  onEnter();
}

bool BmpViewerActivity::openSibling(const int delta, const bool wrap) {
  const int count = static_cast<int>(siblingImages.size());
  if (currentImageIndex < 0 || count <= 1) return false;
  int nextIndex = currentImageIndex + delta;
  if (wrap && (nextIndex < 0 || nextIndex >= count)) nextIndex = (nextIndex + count) % count;
  if (nextIndex < 0 || nextIndex >= count) return false;

  currentImageIndex = nextIndex;
  std::string dirPath = FsHelpers::extractFolderPath(filePath);
  if (!dirPath.empty() && dirPath.back() != '/') dirPath += "/";
  filePath = dirPath + siblingImages[static_cast<size_t>(currentImageIndex)];
  return renderCurrentImage();
}

void BmpViewerActivity::showSlideshowError() {
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  const int height = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawCenteredText(UI_10_FONT_ID, height / 2 - lineHeight, tr(STR_SLIDESHOW_ERROR));
  const char* filename = std::strrchr(filePath.c_str(), '/');
  filename = filename ? filename + 1 : filePath.c_str();
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, filename, std::max(1, renderer.getScreenWidth() - 40), 2);
  for (size_t i = 0; i < lines.size(); ++i) {
    renderer.drawCenteredText(UI_10_FONT_ID, height / 2 + static_cast<int>(i) * lineHeight, lines[i].c_str());
  }
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void BmpViewerActivity::openIntervalPicker() {
  auto activity = makeUniqueNoThrow<IntervalSelectionActivity>(
      renderer, mappedInput, "SlideshowInterval", StrId::STR_SLIDESHOW_INTERVAL, SETTINGS.slideshowIntervalSeconds,
      CrossPointSettings::MIN_SLIDESHOW_INTERVAL_SECONDS, CrossPointSettings::MAX_SLIDESHOW_INTERVAL_SECONDS, 10, 60,
      StrId::STR_NONE_OPT, false, StrId::STR_NONE_OPT, true);
  if (!activity) {
    LOG_ERR("BMP", "OOM: slideshow interval picker");
    return;
  }
  startActivityForResult(std::move(activity), [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      const uint16_t selected = static_cast<uint16_t>(std::get<IntervalResult>(result.data).value);
      if (selected != SETTINGS.slideshowIntervalSeconds) {
        SETTINGS.slideshowIntervalSeconds = selected;
        SETTINGS.saveToFile();
      }
    }
    requestUpdate();
  });
}

void BmpViewerActivity::loop() {
  Activity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (slideshowActive) {
      slideshowActive = false;
      renderCurrentImage();
      return;
    }
    activityManager.goToFileBrowser(filePath);
    return;
  }

  if (slideshowActive) {
    const uint32_t intervalMs = static_cast<uint32_t>(SETTINGS.slideshowIntervalSeconds) * 1000U;
    if (siblingImages.size() > 1 && static_cast<uint32_t>(millis() - lastSlideDisplayMs) >= intervalMs) {
      if (openSibling(1, true)) {
        lastSlideDisplayMs = millis();
      } else {
        slideshowActive = false;
        slideshowError = true;
        showSlideshowError();
      }
    }
    return;
  }
  if (slideshowError) return;

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left) {
    openSibling(1);
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Right) {
    openSibling(-1);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!canSetSleepCover()) return;

    auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_SET_SLEEP_COVER_CONFIRM),
                                                                filePath.substr(filePath.find_last_of('/') + 1));
    if (!confirmation) {
      LOG_ERR("BMP", "OOM: sleep cover confirmation (%zu bytes)", sizeof(ConfirmationActivity));
      return;
    }
    startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
      if (result.isCancelled) {
        onEnter();
        return;
      }
      doSetSleepCover();
    });
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (siblingImages.size() > 1 && currentImageIndex >= 0) {
      slideshowActive = true;
      if (renderCurrentImage()) {
        lastSlideDisplayMs = millis();
      } else {
        slideshowActive = false;
        slideshowError = true;
        showSlideshowError();
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    openIntervalPicker();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    openSibling(-1);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    openSibling(1);
    return;
  }
}
