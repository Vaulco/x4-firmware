#include "XtcReaderActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Serialization.h>

#include "Settings.h"
#include "ChapterSelection.h"

static constexpr int REFRESH_FREQUENCY = 15;

void XtcReaderActivity::taskTrampoline(void* param) {
  auto* self = static_cast<XtcReaderActivity*>(param);
  self->displayTaskLoop();
}

void XtcReaderActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  if (!xtc) {
    return;
  }

  renderingMutex = xSemaphoreCreateMutex();

  xtc->setupCacheDir();

  // Load saved progress
  loadProgress();

  // Save current XTC as last opened book
  SETTINGS.openBookPath = xtc->getPath();
  SETTINGS.saveToFile();

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&XtcReaderActivity::taskTrampoline, "XtcReaderActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void XtcReaderActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
  xtc.reset();
}

void XtcReaderActivity::loop() {
  // Pass input responsibility to sub activity if exists
  if (subActivity) {
    subActivity->loop();
    return;
  }

  // Enter chapter selection activity
  if (inputManager.wasReleased(InputManager::Button::Confirm)) {
    if (xtc && xtc->hasChapters() && !xtc->getChapters().empty()) {
      exitActivity();
      enterNewActivity(new ChapterSelection(
          this->renderer, this->inputManager, xtc, currentPage,
          [this] {
            exitActivity();
            updateRequired = true;
          },
          [this](const uint32_t newPage) {
            currentPage = newPage;
            exitActivity();
            updateRequired = true;
          }));
    }
  }

  // BACK goes to file selection
  if (inputManager.wasReleased(InputManager::Button::Back)) {
    onGoBackToFileSelection();
    return;
  }

  const bool prevReleased = inputManager.wasReleased(InputManager::Button::PageBack) ||
                            inputManager.wasReleased(InputManager::Button::Left);
  const bool nextReleased = inputManager.wasReleased(InputManager::Button::PageForward) ||
                            inputManager.wasReleased(InputManager::Button::Right);

  if (!prevReleased && !nextReleased) {
    return;
  }

  if (prevReleased) {
    if (currentPage > 0) {
      currentPage--;
      updateRequired = true;
    }
  } else if (nextReleased) {
    if (currentPage < xtc->getPageCount() - 1) {
      currentPage++;
      updateRequired = true;
    }
  }
}

void XtcReaderActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      if (xtc) {
        renderPage();
        saveProgress();
      }
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void XtcReaderActivity::renderPage() {
  const uint16_t pageWidth = xtc->getPageWidth();
  const uint16_t pageHeight = xtc->getPageHeight();
  const uint8_t bitDepth = xtc->getBitDepth();

  // Calculate buffer size for one page
  // XTG (1-bit): Row-major, ((width+7)/8) * height bytes
  // XTH (2-bit): Two bit planes, column-major, ((width * height + 7) / 8) * 2 bytes
  const size_t pageBufferSize = (bitDepth == 2)
      ? ((static_cast<size_t>(pageWidth) * pageHeight + 7) / 8) * 2
      : ((pageWidth + 7) / 8) * pageHeight;

  uint8_t* pageBuffer = static_cast<uint8_t*>(malloc(pageBufferSize));
  if (!pageBuffer) {
    Serial.printf("[%lu] [XTR] ERROR: Failed to allocate %lu bytes for page buffer\n", millis(), pageBufferSize);
    return;
  }

  const size_t bytesRead = xtc->loadPage(currentPage, pageBuffer, pageBufferSize);
  if (bytesRead == 0) {
    Serial.printf("[%lu] [XTR] ERROR: Failed to load page %lu\n", millis(), currentPage);
    free(pageBuffer);
    return;
  }

  renderer.clearScreen();

  if (bitDepth == 2) {
    // XTH 2-bit mode: Two bit planes, column-major order
    // - Columns scanned right to left (x = width-1 down to 0)
    // - 8 vertical pixels per byte (MSB = topmost pixel in group)
    // - Pixel value = (bit1 << 1) | bit2
    // - Grayscale: 0=White, 1=Dark Grey, 2=Light Grey, 3=Black

    const size_t planeSize = (static_cast<size_t>(pageWidth) * pageHeight + 7) / 8;
    const uint8_t* plane1 = pageBuffer;
    const uint8_t* plane2 = pageBuffer + planeSize;
    const size_t colBytes = (pageHeight + 7) / 8;

    auto getPixelValue = [&](uint16_t x, uint16_t y) -> uint8_t {
      const size_t colIndex = pageWidth - 1 - x;
      const size_t byteInCol = y / 8;
      const size_t bitInByte = 7 - (y % 8);
      const size_t byteOffset = colIndex * colBytes + byteInCol;
      const uint8_t bit1 = (plane1[byteOffset] >> bitInByte) & 1;
      const uint8_t bit2 = (plane2[byteOffset] >> bitInByte) & 1;
      return (bit1 << 1) | bit2;
    };

    // Pass 1: BW — draw all non-white pixels as black
    for (uint16_t y = 0; y < pageHeight; y++) {
      for (uint16_t x = 0; x < pageWidth; x++) {
        if (getPixelValue(x, y) >= 1) {
          renderer.drawPixel(x, y, true);
        }
      }
    }

    if (pagesUntilFullRefresh <= 1) {
      renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
      pagesUntilFullRefresh = REFRESH_FREQUENCY;
    } else {
      renderer.displayBuffer();
      pagesUntilFullRefresh--;
    }

    // Pass 2: LSB — dark grey only (value 1)
    renderer.clearScreen(0x00);
    for (uint16_t y = 0; y < pageHeight; y++) {
      for (uint16_t x = 0; x < pageWidth; x++) {
        if (getPixelValue(x, y) == 1) {
          renderer.drawPixel(x, y, false);
        }
      }
    }
    renderer.copyGrayscaleLsbBuffers();

    // Pass 3: MSB — dark grey or light grey (values 1 or 2)
    renderer.clearScreen(0x00);
    for (uint16_t y = 0; y < pageHeight; y++) {
      for (uint16_t x = 0; x < pageWidth; x++) {
        const uint8_t pv = getPixelValue(x, y);
        if (pv == 1 || pv == 2) {
          renderer.drawPixel(x, y, false);
        }
      }
    }
    renderer.copyGrayscaleMsbBuffers();

    // Display grayscale overlay
    renderer.displayGrayBuffer();

    // Pass 4: Re-render BW to framebuffer so RED RAM is correct for next fast refresh
    renderer.clearScreen();
    for (uint16_t y = 0; y < pageHeight; y++) {
      for (uint16_t x = 0; x < pageWidth; x++) {
        if (getPixelValue(x, y) >= 1) {
          renderer.drawPixel(x, y, true);
        }
      }
    }
    renderer.cleanupGrayscaleWithFrameBuffer();

    Serial.printf("[%lu] [XTR] Rendered page %lu/%lu (%u-bit grayscale)\n", millis(), currentPage + 1,
                  xtc->getPageCount(), bitDepth);
  } else {
    // 1-bit mode: row-major, 8 pixels per byte, MSB first, 0=black 1=white
    const size_t srcRowBytes = (pageWidth + 7) / 8;

    for (uint16_t srcY = 0; srcY < pageHeight; srcY++) {
      const size_t srcRowStart = srcY * srcRowBytes;
      for (uint16_t srcX = 0; srcX < pageWidth; srcX++) {
        const bool isBlack = !((pageBuffer[srcRowStart + srcX / 8] >> (7 - srcX % 8)) & 1);
        if (isBlack) {
          renderer.drawPixel(srcX, srcY, true);
        }
      }
    }

    if (pagesUntilFullRefresh <= 1) {
      renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
      pagesUntilFullRefresh = REFRESH_FREQUENCY;
    } else {
      renderer.displayBuffer();
      pagesUntilFullRefresh--;
    }

    Serial.printf("[%lu] [XTR] Rendered page %lu/%lu (%u-bit)\n", millis(), currentPage + 1, xtc->getPageCount(),
                  bitDepth);
  }

  free(pageBuffer);
}

void XtcReaderActivity::saveProgress() const {
  FsFile f;
  if (SdMan.openFileForWrite("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    serialization::writePod(f, currentPage);
    f.close();
  }
}

void XtcReaderActivity::loadProgress() {
  FsFile f;
  if (SdMan.openFileForRead("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    if (serialization::readPod(f, currentPage)) {
      Serial.printf("[%lu] [XTR] Loaded progress: page %lu\n", millis(), currentPage + 1);
      if (currentPage >= xtc->getPageCount()) {
        currentPage = 0;
      }
    }
    f.close();
  }
}