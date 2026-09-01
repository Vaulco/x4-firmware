#pragma once
#include <Xtc.h>
#include <memory>
#include <GfxRenderer.h>
#include "activities/util/SelectionActivity.h"

class ChapterSelection final : public SelectionActivity {
    std::shared_ptr<Xtc> xtc;
    std::function<void()> onGoBackCallback;
    std::function<void(uint32_t newPage)> onSelectPage;

    static constexpr int INDENT_STEP = 10;
    static constexpr int PAGE_NUM_RIGHT_MARGIN = 20;
    static constexpr int TITLE_PAGE_GAP = 15;

    int findChapterIndexForPage(uint32_t page) const {
        if (!xtc) return -1;

        const auto& chapters = xtc->getChapters();
        for (size_t i = 0; i < chapters.size(); ++i) {
            if (page >= chapters[i].startPage && page <= chapters[i].endPage) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

protected:
    int getItemCount() const override {
        return xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
    }

    void renderItem(int index, int x, int y, bool isSelected) const override {
        if (!xtc) return;

        const auto& chapters = xtc->getChapters();
        if (index < 0 || index >= static_cast<int>(chapters.size())) return;

        const auto& chapter = chapters[index];
        const int pageWidth = renderer.getScreenWidth();

        xtc::PageInfo pageInfo;
        const uint8_t headerLevel = xtc->getPageInfo(chapter.startPage, pageInfo) ? pageInfo.headerLevel : 0;
        const int indent = headerLevel > 1 ? (headerLevel - 1) * INDENT_STEP : 0;
        const int leftMargin = x + indent;

        char pageNum[16];
        snprintf(pageNum, sizeof(pageNum), "%u", chapter.startPage + 1);
        const int pageNumWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, pageNum);

        const int availableWidth = pageWidth - leftMargin - pageNumWidth - TITLE_PAGE_GAP - PAGE_NUM_RIGHT_MARGIN;

        std::string displayTitle = chapter.name.empty() ? "Unnamed" : chapter.name;
        displayTitle = renderer.truncatedText(GfxRenderer::MEDIUM, displayTitle.c_str(), availableWidth);

        renderer.drawText(GfxRenderer::MEDIUM, leftMargin, y, displayTitle.c_str(), !isSelected);
        renderer.drawText(GfxRenderer::MEDIUM, pageWidth - PAGE_NUM_RIGHT_MARGIN - pageNumWidth, y, pageNum, !isSelected);
    }

    void onItemSelected(int index) override {
        if (!xtc) return;
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
                              std::function<void()> onGoBack,
                              std::function<void(uint32_t newPage)> onSelectPage)
        : SelectionActivity("ChapterSelection", "Select Chapter", renderer, inputManager),
          xtc(xtc),
          onGoBackCallback(std::move(onGoBack)),
          onSelectPage(std::move(onSelectPage))
    {
        selectedIndex = findChapterIndexForPage(currentPage);
    }
};