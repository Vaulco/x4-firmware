#pragma once
#include <EInkDisplay.h>
#include <string>
#include <utility>
#include "../Activity.h"
#include "GfxRenderer.h"
#include "fontIds.h"

class FullScreenMessageActivity final : public Activity {
  std::string text;
  EInkDisplay::RefreshMode refreshMode;

 public:
  explicit FullScreenMessageActivity(GfxRenderer& renderer, InputManager& inputManager, std::string text,
                                     const EInkDisplay::RefreshMode refreshMode = EInkDisplay::FAST_REFRESH)
      : Activity("FullScreenMessage", renderer, inputManager),
        text(std::move(text)),
        refreshMode(refreshMode) {}
  
  void onEnter() override {
    Activity::onEnter();

    const auto height = renderer.getLineHeight(CMU_10_FONT_ID);
    const auto top = (renderer.getScreenHeight() - height) / 2;

    renderer.clearScreen();
    renderer.drawCenteredText(CMU_10_FONT_ID, top, text.c_str(), true);
    renderer.displayBuffer(refreshMode);
  }
};