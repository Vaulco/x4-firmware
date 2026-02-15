#pragma once
#include <functional>
#include <string>
#include <vector>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"

class FileSelectionActivity final : public SelectionActivity {
  std::string basepath = "/";
  std::vector<std::string> files;
  const std::function<void(const std::string&)> onSelect;
  const std::function<void()> onGoToSettings;

  void loadFiles();

 protected:
  // SelectionActivity overrides
  int getItemCount() const override { return static_cast<int>(files.size()); }
  
  void renderItem(int index, int x, int y, bool isSelected) const override {
    auto item = renderer.truncatedText(GfxRenderer::MEDIUM, files[index].c_str(), 
                                      renderer.getScreenWidth() - 40);
    renderer.drawText(GfxRenderer::MEDIUM, x, y, item.c_str(), !isSelected);
  }

  void onItemSelected(int index) override {
    if (files.empty()) {
      return;
    }

    if (basepath.back() != '/') basepath += "/";
    if (files[index].back() == '/') {
      // Navigate into directory
      basepath += files[index].substr(0, files[index].length() - 1);
      loadFiles();
      selectedIndex = 0;
      updateRequired = true;
    } else {
      // Select file
      onSelect(basepath + files[index]);
    }
  }

  void onBack() override {
    if (basepath != "/") {
      // Go up one directory
      basepath.replace(basepath.find_last_of('/'), std::string::npos, "");
      if (basepath.empty()) basepath = "/";
      loadFiles();
      selectedIndex = 0;
      updateRequired = true;
    } else {
      // At root - go to Settings
      onGoToSettings();
    }
  }

 public:
  explicit FileSelectionActivity(GfxRenderer& renderer, InputManager& inputManager,
                                 const std::function<void(const std::string&)>& onSelect,
                                 const std::function<void()>& onGoToSettings,
                                 std::string initialPath = "/")
      : SelectionActivity("FileSelection", "Library", renderer, inputManager),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)),
        onSelect(onSelect),
        onGoToSettings(onGoToSettings) {}

  void onEnter() override {
    loadFiles();
    SelectionActivity::onEnter();
  }
};