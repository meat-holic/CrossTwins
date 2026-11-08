#include "ScreenshotUtil.h"

#include <Arduino.h>
#include <BitmapHelpers.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "Bitmap.h"  // Required for BmpHeader struct definition
#include "activities/Activity.h"
#include "util/ScreenshotFilename.h"

namespace {
constexpr char kScreenshotRoot[] = "/screenshots";
constexpr size_t kScreenshotDirectorySize = 80;
constexpr size_t kScreenshotPathSize = 256;
// FAT long filenames allow at most 255 UTF-16 code units (765 UTF-8 bytes) plus NUL.
constexpr size_t kScreenshotEntryNameSize = 768;

struct ScreenshotSequenceState {
  char directory[kScreenshotDirectorySize] = {};
  uint64_t nextNumber = 1;
  bool initialized = false;
  bool exhausted = false;
};

ScreenshotSequenceState screenshotSequence;

enum class BmpSaveResult { Saved, Collision, Failed };

bool buildScreenshotDirectory(const ScreenshotInfo& info, char* directory, const size_t directorySize) {
  if (info.readerType == ScreenshotInfo::ReaderType::None || info.title[0] == '\0') {
    const int length = std::snprintf(directory, directorySize, "%s", kScreenshotRoot);
    return length >= 0 && static_cast<size_t>(length) < directorySize;
  }

  char sanitizedTitle[64];
  FsHelpers::sanitizePathComponentForFat32(info.title, sanitizedTitle, sizeof(sanitizedTitle));
  if (sanitizedTitle[0] == '\0') {
    const int length = std::snprintf(directory, directorySize, "%s", kScreenshotRoot);
    return length >= 0 && static_cast<size_t>(length) < directorySize;
  }

  const int length = std::snprintf(directory, directorySize, "%s/%s", kScreenshotRoot, sanitizedTitle);
  return length >= 0 && static_cast<size_t>(length) < directorySize;
}

bool ensureScreenshotDirectory(const char* directory) {
  if (!Storage.ready()) return false;
  if (!Storage.exists(directory) && !Storage.mkdir(directory) && !Storage.exists(directory)) {
    LOG_ERR("SCR", "Failed to create screenshot directory: %s", directory);
    return false;
  }

  return true;
}

bool scanLargestScreenshotNumber(const char* directoryPath, uint64_t& largestNumber) {
  if (!ensureScreenshotDirectory(directoryPath)) return false;

  HalFile directory = Storage.open(directoryPath, O_RDONLY);
  if (!directory || !directory.isDirectory()) {
    LOG_ERR("SCR", "Failed to open screenshot directory: %s", directoryPath);
    return false;
  }

  largestNumber = 0;
  while (true) {
    HalFile entry = directory.openNextFile();
    if (!entry) {
      if (directory.getError() != 0 || !Storage.ready()) {
        directory.close();
        LOG_ERR("SCR", "Failed while scanning screenshot directory: %s", directoryPath);
        return false;
      }
      break;
    }

    bool entryOk = true;
    if (!entry.isDirectory()) {
      char filename[kScreenshotEntryNameSize] = {};
      const size_t length = entry.getName(filename, sizeof(filename));
      entryOk = length > 0 && length < sizeof(filename) && screenshot_filename::observeName(filename, largestNumber);
    }

    const bool entryClosed = entry.close();
    if (!entryOk || !entryClosed) {
      directory.close();
      LOG_ERR("SCR", "Failed to inspect screenshot directory: %s", directoryPath);
      return false;
    }
  }

  if (directory.getError() != 0 || !Storage.ready()) {
    directory.close();
    LOG_ERR("SCR", "Screenshot directory scan did not complete: %s", directoryPath);
    return false;
  }

  if (!directory.close() || !Storage.ready()) {
    LOG_ERR("SCR", "Failed to close screenshot directory: %s", directoryPath);
    return false;
  }
  return true;
}

bool initializeScreenshotSequence(const char* directory) {
  if (screenshotSequence.initialized && std::strcmp(screenshotSequence.directory, directory) == 0) {
    if (screenshotSequence.exhausted) {
      LOG_ERR("SCR", "Screenshot sequence exhausted in %s", directory);
      return false;
    }
    return true;
  }

  const size_t directoryLength = std::strlen(directory);
  if (directoryLength >= sizeof(screenshotSequence.directory)) {
    LOG_ERR("SCR", "Screenshot directory path too long");
    return false;
  }

  uint64_t largestNumber = 0;
  if (!scanLargestScreenshotNumber(directory, largestNumber)) return false;

  uint64_t nextNumber = 0;
  std::memcpy(screenshotSequence.directory, directory, directoryLength + 1);
  screenshotSequence.initialized = true;
  screenshotSequence.exhausted = !screenshot_filename::nextAfter(largestNumber, nextNumber);
  screenshotSequence.nextNumber = nextNumber;
  if (screenshotSequence.exhausted) {
    LOG_ERR("SCR", "No screenshot number remains in %s", directory);
    return false;
  }
  return true;
}

bool selectNextScreenshotPath(char* filename, const size_t filenameSize) {
  while (!screenshotSequence.exhausted) {
    if (!screenshot_filename::formatPath(screenshotSequence.directory, screenshotSequence.nextNumber, filename,
                                         filenameSize)) {
      LOG_ERR("SCR", "Screenshot filename buffer is too small");
      return false;
    }
    if (!Storage.exists(filename)) return true;
    if (!screenshot_filename::increment(screenshotSequence.nextNumber)) {
      screenshotSequence.exhausted = true;
      break;
    }
  }
  LOG_ERR("SCR", "No screenshot number remains in %s", screenshotSequence.directory);
  return false;
}

BmpSaveResult saveFramebufferAsBmpExclusive(const char* filename, const uint8_t* framebuffer, int width, int height) {
  if (!framebuffer) return BmpSaveResult::Failed;

  // Keep the current screenshot directory layout while creating missing folders.
  std::string path(filename);
  size_t last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir = path.substr(0, last_slash);
    if (!Storage.exists(dir.c_str())) {
      if (!Storage.mkdir(dir.c_str()) && !Storage.exists(dir.c_str())) {
        return BmpSaveResult::Failed;
      }
    }
  }

  // O_EXCL makes the collision check atomic with file creation. Never truncate
  // an existing screenshot, including one created after the directory scan.
  HalFile file = Storage.open(filename, O_WRONLY | O_CREAT | O_EXCL);
  if (!file) return Storage.exists(filename) ? BmpSaveResult::Collision : BmpSaveResult::Failed;

  // Note: the width and height, we rotate the image 90d counter-clockwise to match the default display orientation
  int phyWidth = height;
  int phyHeight = width;

  BmpHeader header;

  createBmpHeader(&header, phyWidth, phyHeight, BmpRowOrder::BottomUp);

  bool write_error = false;
  if (file.write(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    write_error = true;
  }

  if (write_error) {
    // Explicitly close() file before calling Storage.remove()
    file.close();
    Storage.remove(filename);
    return BmpSaveResult::Failed;
  }

  const uint32_t rowSizePadded = (phyWidth + 31) / 32 * 4;
  // Max row size for 528px height (X3) after rotation = 68 bytes; use fixed buffer to avoid VLA
  constexpr size_t kMaxRowSize = 68;
  if (rowSizePadded > kMaxRowSize) {
    LOG_ERR("SCR", "Row size %u exceeds buffer capacity", rowSizePadded);
    // Explicitly close() file before calling Storage.remove()
    file.close();
    Storage.remove(filename);
    return BmpSaveResult::Failed;
  }

  // rotate the image 90d counter-clockwise on-the-fly while writing to save memory
  uint8_t rowBuffer[kMaxRowSize];
  memset(rowBuffer, 0, rowSizePadded);

  for (int outY = 0; outY < phyHeight; outY++) {
    for (int outX = 0; outX < phyWidth; outX++) {
      // 90d counter-clockwise: source (srcX, srcY)
      // BMP rows are bottom-to-top, so outY=0 is the bottom of the displayed image
      int srcX = width - 1 - outY;     // phyHeight == width
      int srcY = phyWidth - 1 - outX;  // phyWidth == height
      int fbIndex = srcY * (width / 8) + (srcX / 8);
      uint8_t pixel = (framebuffer[fbIndex] >> (7 - (srcX % 8))) & 0x01;
      rowBuffer[outX / 8] |= pixel << (7 - (outX % 8));
    }
    if (file.write(rowBuffer, rowSizePadded) != rowSizePadded) {
      write_error = true;
      break;
    }
    memset(rowBuffer, 0, rowSizePadded);  // Clear the buffer for the next row
  }

  // Explicitly close() file before calling Storage.remove()
  const bool closed = file.close();

  if (write_error || !closed) {
    Storage.remove(filename);
    return BmpSaveResult::Failed;
  }

  return BmpSaveResult::Saved;
}
}  // namespace

bool ScreenshotUtil::takeScreenshot(GfxRenderer& renderer) {
  const uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) {
    LOG_ERR("SCR", "Framebuffer not available");
    return false;
  }

  const ScreenshotInfo info = activityManager.getScreenshotInfo();
  char directory[kScreenshotDirectorySize];
  if (!buildScreenshotDirectory(info, directory, sizeof(directory)) || !initializeScreenshotSequence(directory)) {
    LOG_ERR("SCR", "Failed to initialize screenshot numbering");
    return false;
  }

  char filename[kScreenshotPathSize];
  while (selectNextScreenshotPath(filename, sizeof(filename))) {
    const BmpSaveResult saveResult =
        saveFramebufferAsBmpExclusive(filename, fb, renderer.getDisplayWidth(), renderer.getDisplayHeight());
    if (saveResult == BmpSaveResult::Collision) {
      if (!screenshot_filename::increment(screenshotSequence.nextNumber)) {
        screenshotSequence.exhausted = true;
        LOG_ERR("SCR", "No screenshot number remains in %s", screenshotSequence.directory);
        return false;
      }
      continue;
    }
    if (saveResult != BmpSaveResult::Saved) {
      LOG_ERR("SCR", "Failed to save screenshot to %s", filename);
      return false;
    }

    if (!screenshot_filename::increment(screenshotSequence.nextNumber)) screenshotSequence.exhausted = true;
    LOG_DBG("SCR", "Screenshot saved to %s", filename);

    // Display a border around the screen to indicate a screenshot was taken
    if (renderer.storeBwBuffer()) {
      int marginTop, marginRight, marginBottom, marginLeft;
      renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
      int width = renderer.getScreenWidth() - marginLeft - marginRight - 1;
      int height = renderer.getScreenHeight() - marginTop - marginBottom - 1;
      // Add extra margin to the border to make it more visible
      renderer.drawRect(marginLeft + 1, marginTop + 1, width - 2, height - 2, 2, true);
      renderer.displayBuffer();
      delay(1000);
      renderer.restoreBwBuffer();
      renderer.displayBuffer(HalDisplay::RefreshMode::HALF_REFRESH);
    }
    return true;
  }
  return false;
}

bool ScreenshotUtil::saveFramebufferAsBmp(const char* filename, const uint8_t* framebuffer, const int width,
                                          const int height) {
  return saveFramebufferAsBmpExclusive(filename, framebuffer, width, height) == BmpSaveResult::Saved;
}
