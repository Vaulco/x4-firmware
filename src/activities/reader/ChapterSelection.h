#pragma once
#include <Xtc.h>

#include <memory>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"

class ChapterSelection final : public SelectionActivity {
  std::shared_ptr<Xtc> xtc;
  uint32_t currentPage = 0;
  const std::function<void()> onGoBackCallback;
  const std::function<void(uint32_t newPage)> onSelectPage;

  int findChapterIndexForPage(uint32_t page) const {
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

 protected:
  int getItemCount() const override {
    return xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
  }

  void renderItem(int index, int x, int y, bool isSelected) const override {
    const auto& chapters = xtc->getChapters();
    if (index < 0 || index >= static_cast<int>(chapters.size())) {
      return;
    }

    const auto& chapter = chapters[index];
    const auto pageWidth = renderer.getScreenWidth();
    
    // Get page info to determine header level
    xtc::PageInfo pageInfo;
    const uint8_t headerLevel = xtc->getPageInfo(chapter.startPage, pageInfo) ? pageInfo.headerLevel : 0;
    
    // Calculate indentation based on header level (0 = no header, 1-6 = H1-H6)
    // H1: no indent, H2: 10px, H3: 20px, etc.
    const int indent = headerLevel > 1 ? (headerLevel - 1) * 10 : 0;
    const int leftMargin = x + indent;
    
    // Prepare page number (to calculate available space for title)
    char pageNum[16];
    snprintf(pageNum, sizeof(pageNum), "%u", chapter.startPage + 1);
    const int pageNumWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, pageNum);
    
    // Calculate available width for chapter title (15px gap from page number for spacing)
    const int availableWidth = pageWidth - leftMargin - pageNumWidth - 15 - 20;
    
    // Truncate chapter title if necessary using renderer's UTF-8-safe truncatedText
    std::string displayTitle = chapter.name.empty() ? "Unnamed" : chapter.name;
    displayTitle = renderer.truncatedText(GfxRenderer::MEDIUM, displayTitle.c_str(), availableWidth);
    
    // Draw chapter title on the left with indentation
    renderer.drawText(GfxRenderer::MEDIUM, leftMargin, y, displayTitle.c_str(), !isSelected);
    
    // Draw page number on the right
    renderer.drawText(GfxRenderer::MEDIUM, pageWidth - 20 - pageNumWidth, y, pageNum, !isSelected);
  }
  
  void onItemSelected(int index) override {
    const auto& chapters = xtc->getChapters();
    if (index >= 0 && index < static_cast<int>(chapters.size())) {
      onSelectPage(chapters[index].startPage);
    }
  }

  void onBack() override {
    onGoBackCallback();
  }

 public:
  explicit ChapterSelection(GfxRenderer& renderer, InputManager& inputManager,
                                   const std::shared_ptr<Xtc>& xtc, uint32_t currentPage,
                                   const std::function<void()>& onGoBack,
                                   const std::function<void(uint32_t newPage)>& onSelectPage)
      : SelectionActivity("ChapterSelection", "Select Chapter", renderer, inputManager),
        xtc(xtc),
        currentPage(currentPage),
        onGoBackCallback(onGoBack),
        onSelectPage(onSelectPage) {}

  void onEnter() override {
    selectedIndex = findChapterIndexForPage(currentPage);
    SelectionActivity::onEnter();
  }
};