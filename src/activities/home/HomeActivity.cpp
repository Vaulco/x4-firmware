#include "HomeActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include "MappedInputManager.h"
#include "ScreenComponents.h"
#include "fontIds.h"

void HomeActivity::taskTrampoline(void* param) {
  auto* self = static_cast<HomeActivity*>(param);
  self->displayTaskLoop();
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Reset selection to first item
  selectorIndex = 0;

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&HomeActivity::taskTrampoline, "HomeActivityTask",
              2048,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void HomeActivity::loop() {
  const bool prevPressed = mappedInput.wasPressed(MappedInputManager::Button::Up) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool nextPressed = mappedInput.wasPressed(MappedInputManager::Button::Down) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Right);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    // Menu: Browse, Settings (2 items)
    if (selectorIndex == 0) {
      onReaderOpen();
    } else if (selectorIndex == 1) {
      onSettingsOpen();
    }
  } else if (prevPressed) {
    selectorIndex = (selectorIndex + 1) % 2;  // 2 menu items
    updateRequired = true;
  } else if (nextPressed) {
    selectorIndex = (selectorIndex + 1) % 2;  // 2 menu items
    updateRequired = true;
  }
}

void HomeActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void HomeActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  constexpr int margin = 20;
  constexpr int bottomMargin = 60;

  // --- Menu tiles (2 items: Browse, Settings) ---
  const int menuTileWidth = pageWidth - 2 * margin;
  constexpr int menuTileHeight = 60;
  constexpr int menuSpacing = 20;
  constexpr int totalMenuHeight = 2 * menuTileHeight + menuSpacing;

  // Center the menu vertically
  int menuStartY = (pageHeight - totalMenuHeight) / 2;

  for (int i = 0; i < 2; ++i) {
    constexpr const char* items[2] = {"Browse files", "Settings"};
    constexpr int tileX = margin;
    const int tileY = menuStartY + i * (menuTileHeight + menuSpacing);
    const bool selected = selectorIndex == i;

    if (selected) {
      renderer.fillRect(tileX, tileY, menuTileWidth, menuTileHeight);
    } else {
      renderer.drawRect(tileX, tileY, menuTileWidth, menuTileHeight);
    }

    const char* label = items[i];
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
    const int textX = tileX + (menuTileWidth - textWidth) / 2;
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int textY = tileY + (menuTileHeight - lineHeight) / 2;

    // Invert text when the tile is selected
    renderer.drawText(UI_12_FONT_ID, textX, textY, label, !selected);
  }

  const auto labels = mappedInput.mapLabels("", "Confirm", "Up", "Down");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  ScreenComponents::drawBattery(renderer, 20, pageHeight - 70);

  renderer.displayBuffer();
}