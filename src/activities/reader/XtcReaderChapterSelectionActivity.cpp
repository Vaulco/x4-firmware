#include "XtcReaderChapterSelectionActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;
}  // namespace

int XtcReaderChapterSelectionActivity::getPageItems() const {
  constexpr int startY = 60;
  constexpr int lineHeight = 30;

  const int screenHeight = renderer.getScreenHeight();
  const int availableHeight = screenHeight - startY;
  int items = availableHeight / lineHeight;
  if (items < 1) {
    items = 1;
  }
  return items;
}

int XtcReaderChapterSelectionActivity::findChapterIndexForPage(uint32_t page) const {
  if (!xtc) {
    return 0;
  }

  const auto& chapters = xtc->getChapters();
  for (size_t i = 0; i < chapters.size(); i++) {
    if (page >= chapters[i].startPage && page <= chapters[i].endPage) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

void XtcReaderChapterSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<XtcReaderChapterSelectionActivity*>(param);
  self->displayTaskLoop();
}

void XtcReaderChapterSelectionActivity::onEnter() {
  Activity::onEnter();

  if (!xtc) {
    return;
  }

  renderingMutex = xSemaphoreCreateMutex();
  selectorIndex = findChapterIndexForPage(currentPage);

  updateRequired = true;
  xTaskCreate(&XtcReaderChapterSelectionActivity::taskTrampoline, "XtcReaderChapterSelectionActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void XtcReaderChapterSelectionActivity::onExit() {
  Activity::onExit();

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void XtcReaderChapterSelectionActivity::loop() {
  const bool prevReleased = inputManager.wasReleased(InputManager::Button::Up) ||
                            inputManager.wasReleased(InputManager::Button::Left);
  const bool nextReleased = inputManager.wasReleased(InputManager::Button::Down) ||
                            inputManager.wasReleased(InputManager::Button::Right);

  const bool skipPage = inputManager.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = getPageItems();

  if (inputManager.wasReleased(InputManager::Button::Confirm)) {
    const auto& chapters = xtc->getChapters();
    if (!chapters.empty() && selectorIndex >= 0 && selectorIndex < static_cast<int>(chapters.size())) {
      onSelectPage(chapters[selectorIndex].startPage);
    }
  } else if (inputManager.wasReleased(InputManager::Button::Back)) {
    onGoBack();
  } else if (prevReleased) {
    const int total = static_cast<int>(xtc->getChapters().size());
    if (total == 0) {
      return;
    }
    if (skipPage) {
      selectorIndex = ((selectorIndex / pageItems - 1) * pageItems + total) % total;
    } else {
      selectorIndex = (selectorIndex + total - 1) % total;
    }
    updateRequired = true;
  } else if (nextReleased) {
    const int total = static_cast<int>(xtc->getChapters().size());
    if (total == 0) {
      return;
    }
    if (skipPage) {
      selectorIndex = ((selectorIndex / pageItems + 1) * pageItems) % total;
    } else {
      selectorIndex = (selectorIndex + 1) % total;
    }
    updateRequired = true;
  }
}

void XtcReaderChapterSelectionActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderScreen();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void XtcReaderChapterSelectionActivity::renderScreen() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const int pageItems = getPageItems();
  renderer.drawCenteredText(CMU_12_FONT_ID, 15, "Select Chapter", true, EpdFontFamily::REGULAR);

  const auto& chapters = xtc->getChapters();
  if (chapters.empty()) {
    renderer.drawCenteredText(CMU_10_FONT_ID, 120, "No chapters");
    renderer.displayBuffer();
    return;
  }

  const auto pageStartIndex = selectorIndex / pageItems * pageItems;
  renderer.fillRect(0, 60 + (selectorIndex % pageItems) * 30 - 2, pageWidth - 1, 30);
  for (int i = pageStartIndex; i < static_cast<int>(chapters.size()) && i < pageStartIndex + pageItems; i++) {
    const auto& chapter = chapters[i];
    const int itemY = 60 + (i % pageItems) * 30;
    const bool isSelected = i == selectorIndex;
    
    // Get page info to determine header level
    xtc::PageInfo pageInfo;
    const uint8_t headerLevel = xtc->getPageInfo(chapter.startPage, pageInfo) ? pageInfo.headerLevel : 0;
    
    // Calculate indentation based on header level (0 = no header, 1-6 = H1-H6)
    // H1: no indent, H2: 10px, H3: 20px, etc.
    const int indent = headerLevel > 1 ? (headerLevel - 1) * 10 : 0;
    const int leftMargin = 20 + indent;
    
    // Prepare page number (to calculate available space for title)
    char pageNum[16];
    snprintf(pageNum, sizeof(pageNum), "%u", chapter.startPage + 1);
    const int pageNumWidth = renderer.getTextWidth(CMU_10_FONT_ID, pageNum);
    
    // Calculate available width for chapter title (10px gap from page number)
    const int availableWidth = pageWidth - leftMargin - pageNumWidth - 10 - 20;  // left margin (with indent) + page number width + 10px gap + right margin
    
    // Truncate chapter title if necessary using renderer's truncate function
    std::string displayTitle = chapter.name.empty() ? "Unnamed" : chapter.name;
    displayTitle = renderer.truncatedText(CMU_10_FONT_ID, displayTitle.c_str(), availableWidth);
    
    // Draw chapter title on the left with indentation
    renderer.drawText(CMU_10_FONT_ID, leftMargin, itemY, displayTitle.c_str(), !isSelected);
    
    // Draw page number on the right
    renderer.drawText(CMU_10_FONT_ID, pageWidth - 20 - pageNumWidth, itemY, pageNum, !isSelected);
  }

  renderer.displayBuffer();
}