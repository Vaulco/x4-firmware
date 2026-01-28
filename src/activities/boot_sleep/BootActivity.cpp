#include "BootActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2, "BOOTING");
  renderer.displayBuffer();
}
