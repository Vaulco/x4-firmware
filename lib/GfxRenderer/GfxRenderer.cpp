#include "GfxRenderer.h"

#include <Utf8.h>
#include <builtinFonts/all.h>

// Constructor initializes font array
GfxRenderer::GfxRenderer(EInkDisplay& einkDisplay) 
  : einkDisplay(einkDisplay),
    fonts{EpdFont(&cmu_8), EpdFont(&cmu_10), EpdFont(&cmu_12)} {}

void GfxRenderer::rotateCoordinates(const int x, const int y, int* rotatedX, int* rotatedY) const {
  // Portrait mode (480x800 logical) → panel (800x480 physical)
  // Rotation: 90 degrees clockwise
  *rotatedX = y;
  *rotatedY = EInkDisplay::DISPLAY_HEIGHT - 1 - x;
}

void GfxRenderer::drawPixel(const int x, const int y, const bool state) const {
  uint8_t* frameBuffer = einkDisplay.getFrameBuffer();

  // Early return if no framebuffer is set
  if (!frameBuffer) {
    Serial.printf("[%lu] [GFX] !! No framebuffer\n", millis());
    return;
  }

  int rotatedX = 0;
  int rotatedY = 0;
  rotateCoordinates(x, y, &rotatedX, &rotatedY);

  // Bounds checking against physical panel dimensions
  if (rotatedX < 0 || rotatedX >= EInkDisplay::DISPLAY_WIDTH || rotatedY < 0 ||
      rotatedY >= EInkDisplay::DISPLAY_HEIGHT) {
    Serial.printf("[%lu] [GFX] !! Outside range (%d, %d) -> (%d, %d)\n", millis(), x, y, rotatedX, rotatedY);
    return;
  }

  // Calculate byte position and bit position
  const uint16_t byteIndex = rotatedY * EInkDisplay::DISPLAY_WIDTH_BYTES + (rotatedX / 8);
  const uint8_t bitPosition = 7 - (rotatedX % 8);  // MSB first

  if (state) {
    frameBuffer[byteIndex] &= ~(1 << bitPosition);  // Clear bit
  } else {
    frameBuffer[byteIndex] |= 1 << bitPosition;  // Set bit
  }
}

int GfxRenderer::getTextWidth(const FontSize size, const char* text) const {
  int w = 0, h = 0;
  fonts[size].getTextDimensions(text, &w, &h);
  return w;
}

void GfxRenderer::drawCenteredText(const FontSize size, const int y, const char* text, const bool black) const {
  const int x = (getScreenWidth() - getTextWidth(size, text)) / 2;
  drawText(size, x, y, text, black);
}

void GfxRenderer::drawText(const FontSize size, const int x, const int y, const char* text, const bool black) const {
  const auto& font = fonts[size];
  const int yPos = y + font.data->ascender;
  int xpos = x;

  // cannot draw a NULL / empty string
  if (text == nullptr || *text == '\0') {
    return;
  }

  // no printable characters
  if (!font.hasPrintableChars(text)) {
    return;
  }

  uint32_t cp;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&text)))) {
    renderChar(&font, cp, &xpos, &yPos, black);
  }
}

void GfxRenderer::drawLine(int x1, int y1, int x2, int y2, const bool state) const {
  if (x1 == x2) {
    if (y2 < y1) {
      std::swap(y1, y2);
    }
    for (int y = y1; y <= y2; y++) {
      drawPixel(x1, y, state);
    }
  } else if (y1 == y2) {
    if (x2 < x1) {
      std::swap(x1, x2);
    }
    for (int x = x1; x <= x2; x++) {
      drawPixel(x, y1, state);
    }
  } else {
    // TODO: Implement
    Serial.printf("[%lu] [GFX] Line drawing not supported\n", millis());
  }
}

void GfxRenderer::drawRect(const int x, const int y, const int width, const int height, const bool state) const {
  drawLine(x, y, x + width - 1, y, state);
  drawLine(x + width - 1, y, x + width - 1, y + height - 1, state);
  drawLine(x + width - 1, y + height - 1, x, y + height - 1, state);
  drawLine(x, y, x, y + height - 1, state);
}

void GfxRenderer::fillRect(const int x, const int y, const int width, const int height, const bool state) const {
  for (int fillY = y; fillY < y + height; fillY++) {
    drawLine(x, fillY, x + width - 1, fillY, state);
  }
}

void GfxRenderer::clearScreen(const uint8_t color) const { einkDisplay.clearScreen(color); }

void GfxRenderer::invertScreen() const {
  uint8_t* buffer = einkDisplay.getFrameBuffer();
  if (!buffer) {
    Serial.printf("[%lu] [GFX] !! No framebuffer in invertScreen\n", millis());
    return;
  }
  for (int i = 0; i < EInkDisplay::BUFFER_SIZE; i++) {
    buffer[i] = ~buffer[i];
  }
}

void GfxRenderer::displayBuffer(const EInkDisplay::RefreshMode refreshMode) const {
  einkDisplay.displayBuffer(refreshMode);
}

std::string GfxRenderer::truncatedText(const FontSize size, const char* text, const int maxWidth) const {
  if (text == nullptr || *text == '\0') {
    return "";
  }
  
  // Check if text fits as-is
  const std::string fullText = text;
  int fullWidth = getTextWidth(size, fullText.c_str());
  if (fullWidth <= maxWidth) {
    return fullText;
  }
  
  const char* ellipsis = "...";
  int ellipsisWidth = getTextWidth(size, ellipsis);
  int availableForText = maxWidth - ellipsisWidth;
  
  if (availableForText <= 0) {
    return ellipsis;
  }
  
  // Iterate through UTF-8 codepoints to find safe truncation point
  const unsigned char* str = reinterpret_cast<const unsigned char*>(text);
  const unsigned char* lastSafePos = str;
  std::string candidate;
  
  while (*str) {
    const unsigned char* nextPos = str;
    uint32_t cp = utf8NextCodepoint(&nextPos);
    
    if (cp == 0) break;  // Invalid or end of string
    
    // Build candidate string up to this codepoint
    candidate = std::string(text, nextPos - reinterpret_cast<const unsigned char*>(text));
    int candidateWidth = getTextWidth(size, candidate.c_str());
    
    if (candidateWidth > availableForText) {
      // This character doesn't fit, use last safe position
      break;
    }
    
    lastSafePos = nextPos;
    str = nextPos;
  }
  
  // Build final truncated string
  if (lastSafePos == reinterpret_cast<const unsigned char*>(text)) {
    return ellipsis;  // Not even one character fits
  }
  
  size_t safeLength = lastSafePos - reinterpret_cast<const unsigned char*>(text);
  return fullText.substr(0, safeLength) + ellipsis;
}

// Portrait mode: 480px wide, 800px tall in logical coordinates
int GfxRenderer::getScreenWidth() const {
  return EInkDisplay::DISPLAY_HEIGHT;  // 480px
}

int GfxRenderer::getScreenHeight() const {
  return EInkDisplay::DISPLAY_WIDTH;  // 800px
}

int GfxRenderer::getSpaceWidth(const FontSize size) const {
  return fonts[size].getGlyph(' ')->advanceX;
}

int GfxRenderer::getFontAscenderSize(const FontSize size) const {
  return fonts[size].data->ascender;
}

int GfxRenderer::getLineHeight(const FontSize size) const {
  return fonts[size].data->advanceY;
}

uint8_t* GfxRenderer::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }

size_t GfxRenderer::getBufferSize() { return EInkDisplay::BUFFER_SIZE; }

void GfxRenderer::grayscaleRevert() const { einkDisplay.grayscaleRevert(); }

void GfxRenderer::copyGrayscaleLsbBuffers() const { einkDisplay.copyGrayscaleLsbBuffers(einkDisplay.getFrameBuffer()); }

void GfxRenderer::copyGrayscaleMsbBuffers() const { einkDisplay.copyGrayscaleMsbBuffers(einkDisplay.getFrameBuffer()); }

void GfxRenderer::displayGrayBuffer() const { einkDisplay.displayGrayBuffer(); }

/**
 * Cleanup grayscale buffers using the current frame buffer.
 * Use this when BW buffer was re-rendered instead of stored/restored.
 */
void GfxRenderer::cleanupGrayscaleWithFrameBuffer() const {
  uint8_t* frameBuffer = einkDisplay.getFrameBuffer();
  if (frameBuffer) {
    einkDisplay.cleanupGrayscaleBuffers(frameBuffer);
  }
}

void GfxRenderer::renderChar(const EpdFont* font, const uint32_t cp, int* x, const int* y,
                             const bool pixelState) const {
  const EpdGlyph* glyph = font->getGlyph(cp);
  if (!glyph) {
    glyph = font->getGlyph('?');
  }

  // No glyph?
  if (!glyph) {
    Serial.printf("[%lu] [GFX] No glyph for codepoint %d\n", millis(), cp);
    return;
  }

  const uint32_t offset = glyph->dataOffset;
  const uint8_t width = glyph->width;
  const uint8_t height = glyph->height;
  const int left = glyph->left;

  const uint8_t* bitmap = &font->data->bitmap[offset];

  // Simple 1-bit rendering
  for (int glyphY = 0; glyphY < height; glyphY++) {
    const int screenY = *y - glyph->top + glyphY;
    for (int glyphX = 0; glyphX < width; glyphX++) {
      const int pixelPosition = glyphY * width + glyphX;
      const int screenX = *x + left + glyphX;

      const uint8_t byte = bitmap[pixelPosition / 8];
      const uint8_t bit_index = 7 - (pixelPosition % 8);

      if ((byte >> bit_index) & 1) {
        drawPixel(screenX, screenY, pixelState);
      }
    }
  }

  *x += glyph->advanceX;
}

void GfxRenderer::getOrientedViewableTRBL(int* outTop, int* outRight, int* outBottom, int* outLeft) const {
  // Portrait mode margins
  *outTop = VIEWABLE_MARGIN_TOP;
  *outRight = VIEWABLE_MARGIN_RIGHT;
  *outBottom = VIEWABLE_MARGIN_BOTTOM;
  *outLeft = VIEWABLE_MARGIN_LEFT;
}