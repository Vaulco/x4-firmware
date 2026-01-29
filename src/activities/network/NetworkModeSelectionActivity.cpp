#include "NetworkModeSelectionActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "Battery.h"
#include "fontIds.h"

namespace {
constexpr int MENU_ITEM_COUNT = 2;
const char* MENU_ITEMS[MENU_ITEM_COUNT] = {"Join a Network", "Create Hotspot"};
}  // namespace

void NetworkModeSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<NetworkModeSelectionActivity*>(param);
  self->displayTaskLoop();
}

void NetworkModeSelectionActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Reset selection
  selectedIndex = 0;

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&NetworkModeSelectionActivity::taskTrampoline, "NetworkModeTask",
              2048,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void NetworkModeSelectionActivity::onExit() {
  Activity::onExit();

  // Wait until not rendering to delete task
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void NetworkModeSelectionActivity::loop() {
  // Handle back button - cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }

  // Handle confirm button - select current option
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const NetworkMode mode = (selectedIndex == 0) ? NetworkMode::JOIN_NETWORK : NetworkMode::CREATE_HOTSPOT;
    onModeSelected(mode);
    return;
  }

  // Handle navigation
  const bool prevPressed = mappedInput.wasPressed(MappedInputManager::Button::Up) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool nextPressed = mappedInput.wasPressed(MappedInputManager::Button::Down) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Right);

  if (prevPressed) {
    selectedIndex = (selectedIndex + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
    updateRequired = true;
  } else if (nextPressed) {
    selectedIndex = (selectedIndex + 1) % MENU_ITEM_COUNT;
    updateRequired = true;
  }
}

void NetworkModeSelectionActivity::displayTaskLoop() {
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

void NetworkModeSelectionActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // Draw header (same style as Settings)
  renderer.drawCenteredText(CMU_12_FONT_ID, 15, "File Transfer", true, EpdFontFamily::REGULAR);

  // Draw selection (same style as Settings - 30px spacing between items)
  renderer.fillRect(0, 60 + selectedIndex * 30 - 2, pageWidth - 1, 30);

  // Draw all menu items (same style as Settings)
  for (int i = 0; i < MENU_ITEM_COUNT; i++) {
    const int itemY = 60 + i * 30;  // 30 pixels between items
    renderer.drawText(CMU_10_FONT_ID, 20, itemY, MENU_ITEMS[i], i != selectedIndex);
  }

  // Draw battery indicator centered at bottom (same as Settings)
  const int batteryWidth = 70;  // Approximate width for battery icon + percentage text
  const int batteryX = (pageWidth - batteryWidth) / 2;
  battery.draw(renderer, batteryX, pageHeight - 30);

  renderer.displayBuffer();
}