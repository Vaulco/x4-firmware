#pragma once
#include <EInkDisplay.h>
#include <EpdFont.h>

class GfxRenderer {
 public:
  enum FontSize { SMALL = 0, MEDIUM = 1, LARGE = 2 };
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };

 private:
  EInkDisplay& einkDisplay;
  RenderMode renderMode;
  EpdFont fonts[3];
  void renderChar(const EpdFont* font, uint32_t cp, int* x, const int* y, bool pixelState) const;
  void rotateCoordinates(int x, int y, int* rotatedX, int* rotatedY) const;

 public:
  explicit GfxRenderer(EInkDisplay& einkDisplay);
  ~GfxRenderer() = default;

  static constexpr int VIEWABLE_MARGIN_TOP = 9;
  static constexpr int VIEWABLE_MARGIN_RIGHT = 3;
  static constexpr int VIEWABLE_MARGIN_BOTTOM = 3;
  static constexpr int VIEWABLE_MARGIN_LEFT = 3;

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

  // Text - now using FontSize enum
  void drawCenteredText(FontSize size, int y, const char* text, bool black = true) const;
  void drawText(FontSize size, int x, int y, const char* text, bool black = true) const;
  int getTextWidth(FontSize size, const char* text) const;
  std::string truncatedText(FontSize size, const char* text, int maxWidth) const;
  int getSpaceWidth(FontSize size) const;
  int getFontAscenderSize(FontSize size) const;
  int getLineHeight(FontSize size) const;

  // Grayscale
  void setRenderMode(RenderMode mode) { this->renderMode = mode; }
  void copyGrayscaleLsbBuffers() const;
  void copyGrayscaleMsbBuffers() const;
  void displayGrayBuffer() const;
  void cleanupGrayscaleWithFrameBuffer() const;

  // Low level
  uint8_t* getFrameBuffer() const;
  static size_t getBufferSize();
  void grayscaleRevert() const;
  void getOrientedViewableTRBL(int* outTop, int* outRight, int* outBottom, int* outLeft) const;
};