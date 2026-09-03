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
    std::vector<std::string> files;
    const std::function<void(const std::string&)> onSelect;
    const std::function<void()> onGoToSettings;
    const std::function<void()> onGoToBible;

    static constexpr const char* BIBLE_ENTRY_NAME = "\xD0\x91\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD1\x8F/"; // "Библия/"
    static constexpr const char* BIBLE_LABEL = "\xD0\x91\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD1\x8F";       // "Библия"

    static void sortFileList(std::vector<std::string>& strs) {
        std::sort(strs.begin(), strs.end(), [](const std::string& a, const std::string& b) {
            bool aDir = a.back() == '/';
            bool bDir = b.back() == '/';
            if (aDir != bDir) return aDir;
            std::string lowerA = a, lowerB = b;
            std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);
            std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);
            return lowerA < lowerB;
        });
    }

    void reloadFiles(const std::string& preselect = "") {
        files.clear();
        selectedIndex = 0;

        if (basepath.empty()) basepath = "/";

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

            if (file.isDirectory()) {
                std::string dirName = fname + "/";
                // "Библия" is only ever excluded at the root listing, since
                // it lives at the true root — it's shown via the header
                // shortcut instead of as a regular list entry there.
                if (basepath == "/" && dirName == BIBLE_ENTRY_NAME) {
                    file.close();
                    continue;
                }
                files.push_back(dirName);
            } else if (xtc::isXtcExtension(fname.c_str())) {
                files.push_back(fname);
            }

            file.close();
        }
        root.close();

        sortFileList(files);

        if (!preselect.empty()) {
            auto it = std::find(files.begin(), files.end(), preselect);
            if (it != files.end()) selectedIndex = it - files.begin();
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
            if (pos == std::string::npos) {
                basepath = "/";
            } else {
                basepath = (pos == 0) ? "/" : basepath.substr(0, pos);
            }
            reloadFiles();
            updateRequired = true;
        } else {
            onGoToSettings();
        }
    }

    // Persistent centered header shortcut. Jumps straight into the Bible
    // grid for the last-used translation (replacing the plain "Library"
    // title while this activity is active).
    bool hasHeaderItem() const override { return true; }

    std::string getHeaderItemLabel() const override { return BIBLE_LABEL; }

    void onHeaderItemSelected() override { onGoToBible(); }

public:
    explicit FileSelection(GfxRenderer& renderer, InputManager& inputManager,
                           const std::function<void(const std::string&)>& onSelect,
                           const std::function<void()>& onGoToSettings,
                           const std::function<void()>& onGoToBible,
                           std::string initialPath = "/", std::string selectedFile = "")
        : SelectionActivity("FileSelection", "Library", renderer, inputManager),
          basepath(initialPath.empty() ? "/" : std::move(initialPath)),
          onSelect(onSelect),
          onGoToSettings(onGoToSettings),
          onGoToBible(onGoToBible)
    {
        reloadFiles(std::move(selectedFile));
    }

    void onEnter() override {
        SelectionActivity::onEnter();
    }
};