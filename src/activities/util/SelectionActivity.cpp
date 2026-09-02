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

  // Clamp selected index to valid range. -1 is reserved for the header item
  // (when present) and is otherwise treated as index 0.
  const int itemCount = getItemCount();
  const int minIndex = hasHeaderItem() ? -1 : 0;
  selectedIndex = std::max(minIndex, std::min(selectedIndex, itemCount - 1));

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
  const bool hasHeader = hasHeaderItem();

  // Always allow back navigation, even with no items
  if (inputManager.wasReleased(InputManager::Button::Back)) {
    onBack();
    return;
  }

  // Early return if no items (header item, if any, is still handled below)
  if (itemCount == 0 && !hasHeader) return;

  // Handle confirm
  if (inputManager.wasReleased(InputManager::Button::Confirm)) {
    if (selectedIndex == -1) {
      onHeaderItemSelected();
    } else if (itemCount > 0) {
      onItemSelected(selectedIndex);
    }
    return;
  }

  // Handle navigation
  const bool prevReleased = inputManager.wasReleased(InputManager::Button::Up) ||
                            inputManager.wasReleased(InputManager::Button::Left);
  const bool nextReleased = inputManager.wasReleased(InputManager::Button::Down) ||
                            inputManager.wasReleased(InputManager::Button::Right);

  if (!prevReleased && !nextReleased) return;

  if (itemCount == 0) {
    // Only the header exists; nothing to navigate to.
    return;
  }

  if (prevReleased) {
    // Move up with wrapping. With a header, the loop is:
    // header -> itemN-1 -> ... -> item0 -> header
    if (hasHeader) {
      selectedIndex = (selectedIndex == -1) ? itemCount - 1 : selectedIndex - 1;
    } else {
      selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
    }
    updateRequired = true;
  } else if (nextReleased) {
    // Move down with wrapping. With a header, the loop is:
    // header -> item0 -> ... -> itemN-1 -> header
    if (hasHeader) {
      selectedIndex = (selectedIndex == itemCount - 1) ? -1 : selectedIndex + 1;
    } else {
      selectedIndex = (selectedIndex + 1) % itemCount;
    }
    updateRequired = true;
  }
}

void SelectionActivity::render() const {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pageItems = getPageItems();
  const int itemCount = getItemCount();

  // Draw header. When a header item is present, its label takes the
  // title's centered spot; otherwise the static title is shown as before.
  const bool hasHeader = hasHeaderItem();
  const bool headerSelected = hasHeader && (selectedIndex == -1);
  const std::string headerText = hasHeader ? getHeaderItemLabel() : title;

  if (headerSelected) {
    // Underline style, distinct from the list's filled-bar selection.
    // Anchored just below the text baseline for a tight, close underline.
    const int textWidth = renderer.getTextWidth(GfxRenderer::LARGE, headerText.c_str());
    const int headerX = (pageWidth - textWidth) / 2;
    renderer.drawText(GfxRenderer::LARGE, headerX, 15, headerText.c_str(), true);

    const int underlineY = 15 + renderer.getFontAscenderSize(GfxRenderer::LARGE) + 2;
    renderer.drawLine(headerX, underlineY, headerX + textWidth, underlineY);
  } else {
    renderer.drawCenteredText(GfxRenderer::LARGE, 15, headerText.c_str(), true);
  }

  // Handle empty list
  if (itemCount == 0) {
    renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight / 2, "No items");
    renderer.displayBuffer();
    return;
  }

  // Calculate which page we're on and what items to show.
  // When the header is selected (-1), keep the list's first page visible.
  const int pageAnchorIndex = std::max(selectedIndex, 0);
  const int pageStartIndex = (pageAnchorIndex / pageItems) * pageItems;
  const int pageEndIndex = std::min(pageStartIndex + pageItems, itemCount);

  // Draw selection highlight (only when a list item, not the header, is selected)
  if (selectedIndex >= 0) {
    const int selectionY = startY + (selectedIndex - pageStartIndex) * lineHeight - 2;
    renderer.fillRect(0, selectionY, pageWidth - 1, lineHeight);
  }

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