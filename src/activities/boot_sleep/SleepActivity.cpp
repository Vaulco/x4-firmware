#include "SleepActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Xtc.h>

#include "fontIds.h"

void SleepActivity::onEnter() {
  Activity::onEnter();
  
  // Try to render custom sleep image, fall back to default text
  if (!tryRenderCustomImage()) {
    const auto pageHeight = renderer.getScreenHeight();
    renderer.clearScreen();
    renderer.drawCenteredText(CMU_8_FONT_ID, pageHeight / 2, "SLEEPING");
    renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
  }
}

bool SleepActivity::tryRenderCustomImage() {
  FsFile imageFile;
  
  // Try to load /sleep.bmp
  if (SdMan.openFileForRead("SLP", "/sleep.bmp", imageFile)) {
    Bitmap bitmap(imageFile);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      Serial.printf("[%lu] [SLP] Loading: /sleep.bmp\n", millis());
      renderBitmapImage(bitmap);
      return true;
    }
  }
  
  return false;
}

void SleepActivity::renderBitmapImage(Bitmap& bitmap) const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  
  // Calculate centered position with aspect ratio preservation
  int x, y;
  float cropX = 0, cropY = 0;
  
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    const float imageRatio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);
    
    if (imageRatio > screenRatio) {
      // Image wider than screen - center vertically
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / imageRatio) / 2);
    } else {
      // Image taller than screen - center horizontally
      x = std::round((pageWidth - pageHeight * imageRatio) / 2);
      y = 0;
    }
  } else {
    // Image smaller than screen - center both directions
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }
  
  // Render black and white layer
  renderer.clearScreen();
  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
  
  // Render grayscale layers if present
  if (bitmap.hasGreyscale()) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();
    
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();
    
    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
}