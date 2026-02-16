#pragma once
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <SDCardManager.h>
#include <Xtc/XtcTypes.h>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"

class FileSelection final : public SelectionActivity {
  std::string basepath = "/";
  std::string initialSelectedFile = "";  // File to select after loading
  std::vector<std::string> files;
  const std::function<void(const std::string&)> onSelect;
  const std::function<void()> onGoToSettings;

  static void sortFileList(std::vector<std::string>& strs) {
    std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
      if (str1.back() == '/' && str2.back() != '/') return true;
      if (str1.back() != '/' && str2.back() == '/') return false;
      return lexicographical_compare(
          begin(str1), end(str1), begin(str2), end(str2),
          [](const char& char1, const char& char2) { return tolower(char1) < tolower(char2); });
    });
  }

  void loadFiles() {
    files.clear();
    selectedIndex = 0;

    auto root = SdMan.open(basepath.c_str());
    if (!root || !root.isDirectory()) {
      if (root) root.close();
      return;
    }

    root.rewindDirectory();

    char name[128];
    for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
      file.getName(name, sizeof(name));
      if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
        file.close();
        continue;
      }

      if (file.isDirectory()) {
        files.emplace_back(std::string(name) + "/");
      } else if (xtc::isXtcExtension(name)) {
        files.emplace_back(std::string(name));
      }
      file.close();
    }
    root.close();
    sortFileList(files);
    
    // After loading and sorting, find and select the initial file if specified
    if (!initialSelectedFile.empty()) {
      auto it = std::find(files.begin(), files.end(), initialSelectedFile);
      if (it != files.end()) {
        selectedIndex = std::distance(files.begin(), it);
        Serial.printf("[%lu] [FS] Selected file: %s at index %d\n", millis(), 
                     initialSelectedFile.c_str(), selectedIndex);
      }
      initialSelectedFile = "";  // Clear so it only applies once
    }
  }

 protected:
  int getItemCount() const override { return static_cast<int>(files.size()); }
  
  void renderItem(int index, int x, int y, bool isSelected) const override {
    auto item = renderer.truncatedText(GfxRenderer::MEDIUM, files[index].c_str(), 
                                      renderer.getScreenWidth() - 40);
    renderer.drawText(GfxRenderer::MEDIUM, x, y, item.c_str(), !isSelected);
  }

  void onItemSelected(int index) override {
    if (files.empty()) return;

    if (basepath.back() != '/') basepath += "/";
    if (files[index].back() == '/') {
      basepath += files[index].substr(0, files[index].length() - 1);
      loadFiles();
      selectedIndex = 0;
      updateRequired = true;
    } else {
      onSelect(basepath + files[index]);
    }
  }

  void onBack() override {
    if (basepath != "/") {
      basepath.replace(basepath.find_last_of('/'), std::string::npos, "");
      if (basepath.empty()) basepath = "/";
      loadFiles();
      selectedIndex = 0;
      updateRequired = true;
    } else {
      onGoToSettings();
    }
  }

 public:
  explicit FileSelection(GfxRenderer& renderer, InputManager& inputManager,
                         const std::function<void(const std::string&)>& onSelect,
                         const std::function<void()>& onGoToSettings,
                         std::string initialPath = "/",
                         std::string selectedFile = "")
      : SelectionActivity("FileSelection", "Library", renderer, inputManager),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)),
        initialSelectedFile(std::move(selectedFile)),
        onSelect(onSelect),
        onGoToSettings(onGoToSettings) {}

  void onEnter() override {
    loadFiles();
    SelectionActivity::onEnter();
  }
};