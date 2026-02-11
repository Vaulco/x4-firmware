#pragma once
#include <EInkDisplay.h>

#include <string>
#include <utility>

#include "../Activity.h"

class FullScreenMessageActivity final : public Activity {
  std::string text;
  EInkDisplay::RefreshMode refreshMode;

 public:
  explicit FullScreenMessageActivity(GfxRenderer& renderer, InputManager& inputManager, std::string text,
                                     const EInkDisplay::RefreshMode refreshMode = EInkDisplay::FAST_REFRESH)
      : Activity("FullScreenMessage", renderer, inputManager),
        text(std::move(text)),
        refreshMode(refreshMode) {}
  void onEnter() override;
};
