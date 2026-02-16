#pragma once
#include <memory>
#include <Xtc.h>

#include "activities/ActivityWithSubactivity.h"
#include "activities/util/FullScreenMessageActivity.h"
#include "FileSelection.h"
#include "XtcReaderActivity.h"

class ReaderActivity final : public ActivityWithSubactivity {
  std::string initialBookPath;
  std::string currentBookPath;  // Track current book path for navigation
  const std::function<void()> onGoBack;

  static std::string extractFolderPath(const std::string& filePath) {
    const auto lastSlash = filePath.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
      return "/";
    }
    return filePath.substr(0, lastSlash);
  }

  static bool isXtcFile(const std::string& path) {
    if (path.length() < 4) return false;
    std::string ext4 = path.substr(path.length() - 4);
    if (ext4 == ".xtc") return true;
    if (path.length() >= 5) {
      std::string ext5 = path.substr(path.length() - 5);
      if (ext5 == ".xtch") return true;
    }
    return false;
  }

  static std::unique_ptr<Xtc> loadXtc(const std::string& path) {
    auto xtc = std::unique_ptr<Xtc>(new Xtc(path, "/.ereader"));
    if (xtc->load()) {
      return xtc;
    }

    Serial.printf("[%lu] [   ] Failed to load XTC\n", millis());
    return nullptr;
  }

  void onSelectBookFile(const std::string& path) {
    currentBookPath = path;  // Track current book path
    exitActivity();

    // Load XTC file
    auto xtc = loadXtc(path);
    if (xtc) {
      onGoToXtcReader(std::move(xtc));
    } else {
      exitActivity();
      enterNewActivity(new FullScreenMessageActivity(renderer, inputManager, "Failed to load XTC",
                                                     EInkDisplay::HALF_REFRESH));
      delay(2000);
      onGoToFileSelection();
    }
  }

  void onGoToFileSelection(const std::string& fromBookPath = "") {
    exitActivity();
    // If coming from a book, start in that book's folder; otherwise start from root
    const auto initialPath = fromBookPath.empty() ? "/" : extractFolderPath(fromBookPath);
    enterNewActivity(new FileSelection(
        renderer, inputManager, [this](const std::string& path) { onSelectBookFile(path); }, onGoBack, initialPath));
  }

  void onGoToXtcReader(std::unique_ptr<Xtc> xtc) {
    const auto xtcPath = xtc->getPath();
    currentBookPath = xtcPath;
    exitActivity();
    enterNewActivity(new XtcReaderActivity(
        renderer, inputManager, std::move(xtc), 
        [this] { onGoToFileSelection(); }));  // Back goes to file selection
  }

 public:
  explicit ReaderActivity(GfxRenderer& renderer, InputManager& inputManager, std::string initialBookPath,
                          const std::function<void()>& onGoBack)
      : ActivityWithSubactivity("Reader", renderer, inputManager),
        initialBookPath(std::move(initialBookPath)),
        onGoBack(onGoBack) {}

  void onEnter() override {
    ActivityWithSubactivity::onEnter();

    if (initialBookPath.empty()) {
      onGoToFileSelection();  // Start from root when entering via Browse
      return;
    }

    currentBookPath = initialBookPath;

    auto xtc = loadXtc(initialBookPath);
    if (!xtc) {
      onGoBack();
      return;
    }
    onGoToXtcReader(std::move(xtc));
  }
};