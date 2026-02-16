#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <SDCardManager.h>
#include <Xtc/XtcTypes.h>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"

class FileSelection final : public SelectionActivity {
    std::string basepath;
    std::string initialSelectedFile;
    std::vector<std::string> files;
    const std::function<void(const std::string&)> onSelect;
    const std::function<void()> onGoToSettings;

    static void sortFileList(std::vector<std::string>& strs) {
        std::sort(strs.begin(), strs.end(), [](const std::string& a, const std::string& b) {
            bool aDir = a.back() == '/';
            bool bDir = b.back() == '/';
            if (aDir != bDir) return aDir; // directories first
            std::string lowerA = a, lowerB = b;
            std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);
            std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);
            return lowerA < lowerB;
        });
    }

    void reloadFiles() {
        files.clear();
        selectedIndex = 0;

        auto root = SdMan.open(basepath.c_str());
        if (!root || !root.isDirectory()) {
            if (root) root.close();
            return;
        }

        char name[128];
        for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
            file.getName(name, sizeof(name));
            std::string fname(name);
            if (fname[0] == '.' || fname == "System Volume Information") {
                file.close();
                continue;
            }

            if (file.isDirectory()) files.push_back(fname + "/");
            else if (xtc::isXtcExtension(fname.c_str())) files.push_back(fname);

            file.close();
        }
        root.close();

        sortFileList(files);

        if (!initialSelectedFile.empty()) {
            auto it = std::find(files.begin(), files.end(), initialSelectedFile);
            if (it != files.end()) selectedIndex = it - files.begin();
            initialSelectedFile.clear();
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

      std::string fullPath = basepath;
      if (fullPath.back() != '/') fullPath += '/';

      if (files[index].back() == '/') {
          basepath = fullPath + files[index].substr(0, files[index].size() - 1);
          reloadFiles();
          updateRequired = true;
      } else {
          onSelect(fullPath + files[index]);
      }
    }

    void onBack() override {
        if (basepath != "/") {
            auto pos = basepath.find_last_of('/');
            basepath.erase(pos);
            if (basepath.empty()) basepath = "/";
            reloadFiles();
            updateRequired = true;
        } else {
            onGoToSettings();
        }
    }

public:
    explicit FileSelection(GfxRenderer& renderer, InputManager& inputManager,
                           const std::function<void(const std::string&)>& onSelect,
                           const std::function<void()>& onGoToSettings,
                           std::string initialPath = "/", std::string selectedFile = "")
        : SelectionActivity("FileSelection", "Library", renderer, inputManager),
          basepath(initialPath.empty() ? "/" : std::move(initialPath)),
          initialSelectedFile(std::move(selectedFile)),
          onSelect(onSelect),
          onGoToSettings(onGoToSettings) {}

    void onEnter() override {
        reloadFiles();
        SelectionActivity::onEnter();
    }
};