#include "XtcReaderChapterSelectionActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;

// Manual truncation - avoid renderer.truncatedText() which has a bug
std::string truncateToWidth(GfxRenderer& renderer, int fontId, const std::string& text, int maxWidth) {
  if (text.empty()) {
    return text;
  }
  
  // Check if text fits as-is
  int fullWidth = renderer.getTextWidth(fontId, text.c_str());
  if (fullWidth <= maxWidth) {
    return text;
  }
  
  // Binary search for the right length
  const char* ellipsis = "...";
  int ellipsisWidth = renderer.getTextWidth(fontId, ellipsis);
  int availableForText = maxWidth - ellipsisWidth;
  
  if (availableForText <= 0) {
    return ellipsis;
  }
  
  // Start with rough estimate based on character average
  size_t estimatedChars = (text.length() * availableForText) / fullWidth;
  if (estimatedChars > text.length()) {
    estimatedChars = text.length();
  }
  
  // Find the longest substring that fits
  size_t low = 0;
  size_t high = estimatedChars;
  size_t best = 0;
  
  while (low <= high && high <= text.length()) {
    size_t mid = (low + high) / 2;
    std::string candidate = text.substr(0, mid);
    int candidateWidth = renderer.getTextWidth(fontId, candidate.c_str());
    
    if (candidateWidth <= availableForText) {
      best = mid;
      low = mid + 1;
    } else {
      if (mid == 0) break;
      high = mid - 1;
    }
  }
  
  if (best == 0) {
    return ellipsis;
  }
  
  return text.substr(0, best) + ellipsis;
}

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

void XtcReaderChapterSelectionActivity::onEnter() {
  Serial.printf("[%lu] [CHAP] onEnter\n", millis());
  Activity::onEnter();

  if (!xtc) {
    return;
  }

  selectorIndex = findChapterIndexForPage(currentPage);
  render();
}

void XtcReaderChapterSelectionActivity::onExit() {
  Activity::onExit();
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
      const uint32_t newPage = chapters[selectorIndex].startPage;
      onSelectPage(newPage);
    }
    return;
  }
  
  if (inputManager.wasReleased(InputManager::Button::Back)) {
    onGoBack();
    return;
  }
  
  if (prevReleased) {
    const int total = static_cast<int>(xtc->getChapters().size());
    if (total == 0) {
      return;
    }
    if (skipPage) {
      selectorIndex = ((selectorIndex / pageItems - 1) * pageItems + total) % total;
    } else {
      selectorIndex = (selectorIndex + total - 1) % total;
    }
    render();
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
    render();
  }
}

void XtcReaderChapterSelectionActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const int pageItems = getPageItems();
  
  renderer.drawCenteredText(CMU_12_FONT_ID, 15, "Select Chapter", true);

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
    
    // Calculate available width for chapter title (15px gap from page number for spacing)
    const int availableWidth = pageWidth - leftMargin - pageNumWidth - 15 - 20;
    
    // Truncate chapter title if necessary using OUR manual truncation (not renderer's buggy one)
    std::string displayTitle = chapter.name.empty() ? "Unnamed" : chapter.name;
    displayTitle = truncateToWidth(renderer, CMU_10_FONT_ID, displayTitle, availableWidth);
    
    // Draw chapter title on the left with indentation
    renderer.drawText(CMU_10_FONT_ID, leftMargin, itemY, displayTitle.c_str(), !isSelected);
    
    // Draw page number on the right
    renderer.drawText(CMU_10_FONT_ID, pageWidth - 20 - pageNumWidth, itemY, pageNum, !isSelected);
  }

  renderer.displayBuffer();
}