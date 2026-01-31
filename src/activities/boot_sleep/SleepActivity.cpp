#include "SleepActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"

void SleepActivity::onEnter() {
  Activity::onEnter();
  
  const auto pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  renderer.drawCenteredText(CMU_8_FONT_ID, pageHeight / 2, "SLEEPING");
  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
}