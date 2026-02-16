#include "SelectionActivity.h"

#include <GfxRenderer.h>

#include "Battery.h"

void SelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<SelectionActivity*>(param);
  self->displayTaskLoop();
}

void SelectionActivity::displayTaskLoop() {
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

int SelectionActivity::getPageItems() const {
  const int screenHeight = renderer.getScreenHeight();
  const int footerHeight = showBatteryIndicator ? 50 : 20;
  const int availableHeight = screenHeight - startY - footerHeight;
  return std::max(1, availableHeight / lineHeight);
}

void SelectionActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Clamp selected index to valid range
  const int itemCount = getItemCount();
  selectedIndex = std::max(0, std::min(selectedIndex, itemCount - 1));

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&SelectionActivity::taskTrampoline, "SelectionTask",
              2048, this, 1, &displayTaskHandle);
}

void SelectionActivity::onExit() {
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

void SelectionActivity::loop() {
  const int itemCount = getItemCount();

  // Always allow back navigation, even with no items
  if (inputManager.wasReleased(InputManager::Button::Back)) {
    onBack();
    return;
  }

  // Early return if no items
  if (itemCount == 0) return;

  // Handle confirm
  if (inputManager.wasReleased(InputManager::Button::Confirm)) {
    onItemSelected(selectedIndex);
    return;
  }

  // Handle navigation
  const bool prevReleased = inputManager.wasReleased(InputManager::Button::Up) ||
                            inputManager.wasReleased(InputManager::Button::Left);
  const bool nextReleased = inputManager.wasReleased(InputManager::Button::Down) ||
                            inputManager.wasReleased(InputManager::Button::Right);

  if (!prevReleased && !nextReleased) return;

  if (prevReleased) {
    // Move up with wrapping
    selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
    updateRequired = true;
  } else if (nextReleased) {
    // Move down with wrapping
    selectedIndex = (selectedIndex + 1) % itemCount;
    updateRequired = true;
  }
}

void SelectionActivity::render() const {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pageItems = getPageItems();
  const int itemCount = getItemCount();

  // Draw header
  renderer.drawCenteredText(GfxRenderer::LARGE, 15, title.c_str(), true);

  // Handle empty list
  if (itemCount == 0) {
    renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight / 2, "No items");
    renderer.displayBuffer();
    return;
  }

  // Calculate which page we're on and what items to show
  const int pageStartIndex = (selectedIndex / pageItems) * pageItems;
  const int pageEndIndex = std::min(pageStartIndex + pageItems, itemCount);

  // Draw selection highlight
  const int selectionY = startY + (selectedIndex - pageStartIndex) * lineHeight - 2;
  renderer.fillRect(0, selectionY, pageWidth - 1, lineHeight);

  // Draw visible items
  for (int i = pageStartIndex; i < pageEndIndex; i++) {
    const int itemY = startY + (i - pageStartIndex) * lineHeight;
    const bool isSelected = (i == selectedIndex);
    renderItem(i, 20, itemY, isSelected);
  }

  // Draw scroll indicators
  if (pageStartIndex > 0) {
    renderer.drawText(GfxRenderer::SMALL, pageWidth - 15, startY - 10, "^");
  }
  if (pageEndIndex < itemCount) {
    renderer.drawText(GfxRenderer::SMALL, pageWidth - 15, startY + pageItems * lineHeight, "v");
  }

  // Draw battery indicator
  if (showBatteryIndicator) {
    const uint16_t batteryPercentage = battery.readPercentage();
    const std::string batteryText = std::to_string(batteryPercentage) + "%";
    renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight - 30, batteryText.c_str());
  }

  renderer.displayBuffer();
}