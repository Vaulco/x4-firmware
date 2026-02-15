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
  const int startY = getStartY();
  const int lineHeight = getLineHeight();
  const int screenHeight = renderer.getScreenHeight();
  const int footerHeight = showBattery() ? 50 : 20;  // Space for battery + margin
  const int availableHeight = screenHeight - startY - footerHeight;
  int items = availableHeight / lineHeight;
  if (items < 1) {
    items = 1;
  }
  return items;
}

void SelectionActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Clamp selected index
  const int itemCount = getItemCount();
  if (selectedIndex >= itemCount) {
    selectedIndex = itemCount > 0 ? itemCount - 1 : 0;
  }

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&SelectionActivity::taskTrampoline, "SelectionTask",
              2048,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
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
  if (itemCount == 0) {
    // Handle back button even with no items
    if (inputManager.wasReleased(InputManager::Button::Back)) {
      onBack();
    }
    return;
  }

  // Handle confirm button
  if (inputManager.wasReleased(InputManager::Button::Confirm)) {
    onItemSelected(selectedIndex);
    return;
  }

  // Handle back button
  if (inputManager.wasReleased(InputManager::Button::Back)) {
    onBack();
    return;
  }

  // Handle navigation
  const bool prevReleased = inputManager.wasReleased(InputManager::Button::Up) ||
                            inputManager.wasReleased(InputManager::Button::Left);
  const bool nextReleased = inputManager.wasReleased(InputManager::Button::Down) ||
                            inputManager.wasReleased(InputManager::Button::Right);

  if (!prevReleased && !nextReleased) {
    return;
  }

  const bool skipPage = inputManager.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = getPageItems();

  if (prevReleased) {
    if (skipPage) {
      selectedIndex = ((selectedIndex / pageItems - 1) * pageItems + itemCount) % itemCount;
    } else {
      selectedIndex = (selectedIndex + itemCount - 1) % itemCount;
    }
    updateRequired = true;
  } else if (nextReleased) {
    if (skipPage) {
      selectedIndex = ((selectedIndex / pageItems + 1) * pageItems) % itemCount;
    } else {
      selectedIndex = (selectedIndex + 1) % itemCount;
    }
    updateRequired = true;
  }
}

void SelectionActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int startY = getStartY();
  const int lineHeight = getLineHeight();
  const int pageItems = getPageItems();

  // Draw header
  renderer.drawCenteredText(GfxRenderer::LARGE, 15, title.c_str(), true);

  // Allow custom header rendering
  renderCustomHeader();

  const int itemCount = getItemCount();

  if (itemCount == 0) {
    renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight / 2, "No items");
    renderer.displayBuffer();
    return;
  }

  // Calculate scroll offset
  const int pageStartIndex = selectedIndex / pageItems * pageItems;

  // Draw selection highlight
  renderer.fillRect(0, startY + (selectedIndex % pageItems) * lineHeight - 2, pageWidth - 1, lineHeight);

  // Draw items
  for (int i = pageStartIndex; i < itemCount && i < pageStartIndex + pageItems; i++) {
    const int itemY = startY + (i % pageItems) * lineHeight;
    const bool isSelected = (i == selectedIndex);
    renderItem(i, 20, itemY, isSelected);
  }

  // Draw scroll indicators if needed
  if (pageStartIndex > 0) {
    renderer.drawText(GfxRenderer::SMALL, pageWidth - 15, startY - 10, "^");
  }
  if (pageStartIndex + pageItems < itemCount) {
    renderer.drawText(GfxRenderer::SMALL, pageWidth - 15, startY + pageItems * lineHeight, "v");
  }

  // Allow custom footer rendering
  renderCustomFooter();

  // Draw battery indicator
  if (showBattery()) {
    const uint16_t batteryPercentage = battery.readPercentage();
    const std::string batteryText = std::to_string(batteryPercentage) + "%";
    renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight - 30, batteryText.c_str());
  }

  renderer.displayBuffer();
}