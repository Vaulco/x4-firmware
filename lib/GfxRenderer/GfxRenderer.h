#pragma once

#include <EInkDisplay.h>
#include <EpdFontFamily.h>

#include <map>

#include "Bitmap.h"

class GfxRenderer {
 public:
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };

 private:
  EInkDisplay& einkDisplay;
  RenderMode renderMode;
  std::map<int, EpdFontFamily> fontMap;
  void renderChar(const EpdFontFamily& fontFamily, uint32_t cp, int* x, const int* y, bool pixelState,
                  EpdFontFamily::Style style) const;
  void rotateCoordinates(int x, int y, int* rotatedX, int* rotatedY) const;

 public:
  explicit GfxRenderer(EInkDisplay& einkDisplay) : einkDisplay(einkDisplay), renderMode(BW) {}
  ~GfxRenderer() = default;

  static constexpr int VIEWABLE_MARGIN_TOP = 9;
  static constexpr int VIEWABLE_MARGIN_RIGHT = 3;
  static constexpr int VIEWABLE_MARGIN_BOTTOM = 3;
  static constexpr int VIEWABLE_MARGIN_LEFT = 3;

  // Setup
  void insertFont(int fontId, EpdFontFamily font);

  // Screen ops
  int getScreenWidth() const;
  int getScreenHeight() const;
  void displayBuffer(EInkDisplay::RefreshMode refreshMode = EInkDisplay::FAST_REFRESH) const;
  void invertScreen() const;
  void clearScreen(uint8_t color = 0xFF) const;

  // Drawing
  void drawPixel(int x, int y, bool state = true) const;
  void drawLine(int x1, int y1, int x2, int y2, bool state = true) const;
  void drawRect(int x, int y, int width, int height, bool state = true) const;
  void fillRect(int x, int y, int width, int height, bool state = true) const;
  void drawBitmap(const Bitmap& bitmap, int x, int y, int maxWidth, int maxHeight, float cropX = 0,
                  float cropY = 0) const;

  // Text
  void drawCenteredText(int fontId, int y, const char* text, bool black = true) const;
  void drawText(int fontId, int x, int y, const char* text, bool black = true) const;
  int getTextWidth(int fontId, const char* text) const;
  std::string truncatedText(int fontId, const char* text, int maxWidth) const;
  int getSpaceWidth(int fontId) const;
  int getFontAscenderSize(int fontId) const;
  int getLineHeight(int fontId) const;

 public:
  // Grayscale functions
  void setRenderMode(const RenderMode mode) { this->renderMode = mode; }
  void copyGrayscaleLsbBuffers() const;
  void copyGrayscaleMsbBuffers() const;
  void displayGrayBuffer() const;
  void cleanupGrayscaleWithFrameBuffer() const;

  // Low level functions
  uint8_t* getFrameBuffer() const;
  static size_t getBufferSize();
  void grayscaleRevert() const;
  void getOrientedViewableTRBL(int* outTop, int* outRight, int* outBottom, int* outLeft) const;
};