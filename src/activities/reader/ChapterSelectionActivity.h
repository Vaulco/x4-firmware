#pragma once
#include <Xtc.h>

#include <memory>

#include "activities/util/SelectionActivity.h"

class ChapterSelectionActivity final : public SelectionActivity {
  std::shared_ptr<Xtc> xtc;
  uint32_t currentPage = 0;
  const std::function<void()> onGoBackCallback;
  const std::function<void(uint32_t newPage)> onSelectPage;

  int findChapterIndexForPage(uint32_t page) const;

 protected:
  int getItemCount() const override {
    return xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
  }

  void renderItem(int index, int x, int y, bool isSelected) const override;
  
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
  explicit ChapterSelectionActivity(GfxRenderer& renderer, InputManager& inputManager,
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