#include "Battery.h"

#include <GfxRenderer.h>

#include <string>

#include "fontIds.h"

void Battery::draw(const GfxRenderer& renderer, int left, int top) const {
  const uint16_t percentage = monitor.readPercentage();

  // Draw percentage text
  const std::string text = std::to_string(percentage) + "%";
  renderer.drawText(CMU_8_FONT_ID, left + 27, top, text.c_str());

  constexpr int bodyW = 20;
  constexpr int bodyH = 12;
  constexpr int tipW  = 2;

  const int x = left;
  const int y = top + 6;

  // Battery body (outline)
  renderer.drawRect(x, y, bodyW, bodyH);

  // Battery tip
  renderer.fillRect(x + bodyW, y + 3, tipW, bodyH - 6);

  // Fill level
  const int maxFill = bodyW - 4;
  int fillW = (percentage * maxFill + 99) / 100; // rounded
  renderer.fillRect(x + 2, y + 2, fillW, bodyH - 4);
}

// Define global instance
Battery battery(BAT_GPIO0);