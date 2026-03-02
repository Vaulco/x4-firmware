#pragma once
#include <functional>
#include <string>
#include <GfxRenderer.h>

#include "activities/Activity.h"

/**
 * Reusable yes/no confirmation dialog activity.
 *
 * Usage:
 *   enterNewActivity(new ConfirmActivity(
 *       renderer, inputManager,
 *       "Save password for next time?",   // question
 *       "Save Password",                  // title (optional, pass "" to omit)
 *       [this](bool confirmed) {
 *           if (confirmed) { ... }
 *           exitActivity();
 *       }
 *   ));
 */
class ConfirmActivity final : public Activity {
  std::string title;
  std::string question;
  std::function<void(bool confirmed)> onResult;

  int selectedIndex = 0;  // 0 = Yes, 1 = No

  static constexpr int OPTION_COUNT = 2;
  static constexpr const char* OPTIONS[OPTION_COUNT] = {"Yes", "No"};

  void render() const {
    renderer.clearScreen();

    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();
    const int centerY = pageHeight / 2;

    // Draw title if provided
    if (!title.empty()) {
      renderer.drawCenteredText(GfxRenderer::LARGE, 15, title.c_str(), true);
    }

    // Draw question - wrap across two lines if too wide
    const int maxWidth = pageWidth - 40;
    const int questionWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, question.c_str());
    if (questionWidth <= maxWidth) {
      renderer.drawCenteredText(GfxRenderer::MEDIUM, centerY - 30, question.c_str());
    } else {
      // Split at the last space before the midpoint
      const size_t mid = question.size() / 2;
      size_t splitPos = question.rfind(' ', mid);
      if (splitPos == std::string::npos) splitPos = question.find(' ');

      if (splitPos != std::string::npos) {
        const std::string line1 = question.substr(0, splitPos);
        const std::string line2 = question.substr(splitPos + 1);
        const int lineHeight = renderer.getLineHeight(GfxRenderer::MEDIUM);
        renderer.drawCenteredText(GfxRenderer::MEDIUM, centerY - 30 - lineHeight / 2, line1.c_str());
        renderer.drawCenteredText(GfxRenderer::MEDIUM, centerY - 30 + lineHeight / 2, line2.c_str());
      } else {
        renderer.drawCenteredText(GfxRenderer::MEDIUM, centerY - 30, question.c_str());
      }
    }

    // Draw Yes / No buttons
    constexpr int BUTTON_SPACING = 60;
    const int totalWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, "[Yes]") + BUTTON_SPACING +
                           renderer.getTextWidth(GfxRenderer::MEDIUM, "[No]");
    const int startX = (pageWidth - totalWidth) / 2;
    const int buttonY = centerY + 20;

    for (int i = 0; i < OPTION_COUNT; i++) {
      const bool isSelected = (i == selectedIndex);
      std::string label = isSelected ? (std::string("[") + OPTIONS[i] + "]") : OPTIONS[i];
      const int xOffset = isSelected ? 0 : 4;  // slight indent for unselected to compensate for missing brackets
      int x = startX + xOffset;
      if (i == 1) {
        x = startX + renderer.getTextWidth(GfxRenderer::MEDIUM, "[Yes]") + BUTTON_SPACING + xOffset;
      }
      renderer.drawText(GfxRenderer::MEDIUM, x, buttonY, label.c_str());
    }

    renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight - 30, "LEFT/RIGHT: Select | OK: Confirm");
    renderer.displayBuffer();
  }

 public:
  explicit ConfirmActivity(GfxRenderer& renderer, InputManager& inputManager,
                           std::string question,
                           std::string title,
                           std::function<void(bool confirmed)> onResult,
                           const int defaultIndex = 0)
      : Activity("Confirm", renderer, inputManager),
        title(std::move(title)),
        question(std::move(question)),
        onResult(std::move(onResult)),
        selectedIndex(defaultIndex) {}

  void onEnter() override {
    Activity::onEnter();
    render();
  }

  void loop() override {
    const bool moveLeft  = inputManager.wasPressed(InputManager::Button::Left) ||
                           inputManager.wasPressed(InputManager::Button::Up);
    const bool moveRight = inputManager.wasPressed(InputManager::Button::Right) ||
                           inputManager.wasPressed(InputManager::Button::Down);

    if (moveLeft && selectedIndex > 0) {
      selectedIndex--;
      render();
    } else if (moveRight && selectedIndex < OPTION_COUNT - 1) {
      selectedIndex++;
      render();
    } else if (inputManager.wasPressed(InputManager::Button::Confirm)) {
      onResult(selectedIndex == 0);
    } else if (inputManager.wasPressed(InputManager::Button::Back)) {
      // Back = cancel = No
      onResult(false);
    }
  }
};