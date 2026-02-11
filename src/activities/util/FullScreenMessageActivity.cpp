#include "FullScreenMessageActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"

void FullScreenMessageActivity::onEnter() {
  Activity::onEnter();

  const auto height = renderer.getLineHeight(CMU_10_FONT_ID);
  const auto top = (renderer.getScreenHeight() - height) / 2;

  renderer.clearScreen();
  renderer.drawCenteredText(CMU_10_FONT_ID, top, text.c_str(), true);
  renderer.displayBuffer(refreshMode);
}
