#pragma once
#include <memory>
#include <Xtc.h>

#include "activities/ActivityWithSubactivity.h"
#include "activities/util/FullScreenMessageActivity.h"
#include "FileSelection.h"
#include "XtcReaderActivity.h"

class ReaderActivity final : public ActivityWithSubactivity {
  std::string currentBookPath;  // Track current book path for navigation
  const std::function<void()> onGoBack;

  static std::unique_ptr<Xtc> loadXtc(const std::string& path) {
      auto xtc = std::make_unique<Xtc>(path, "/.ereader");
      if (!xtc->load()) return nullptr;
      return xtc;
  }

  void showErrorAndReturnToFileSelection() {
    enterNewActivity(new FullScreenMessageActivity(renderer, inputManager, "Failed to load XTC",
                                                   EInkDisplay::HALF_REFRESH));
    delay(2000);
    goToFileSelection();
  }

  void goToFileSelection(const std::string& fromBookPath = "") {
    exitActivity();
    
    // Extract folder and filename from book path
    std::string folder = "/";
    std::string filename = "";
    
    if (!fromBookPath.empty()) {
      const auto lastSlash = fromBookPath.find_last_of('/');
      if (lastSlash != std::string::npos) {
        folder = (lastSlash == 0) ? "/" : fromBookPath.substr(0, lastSlash);
        filename = fromBookPath.substr(lastSlash + 1);
      }
    }
    
    enterNewActivity(new FileSelection(
        renderer, inputManager, 
        [this](const std::string& path) { openBook(path); }, 
        onGoBack, 
        folder,
        filename));
  }

  void openBook(const std::string& path) {
    currentBookPath = path;
    exitActivity();

    auto xtc = loadXtc(path);
    if (!xtc) {
      showErrorAndReturnToFileSelection();
      return;
    }

    enterNewActivity(new XtcReaderActivity(
        renderer, inputManager, std::move(xtc), 
        [this] { goToFileSelection(currentBookPath); }));
  }

 public:
  explicit ReaderActivity(GfxRenderer& renderer, InputManager& inputManager, std::string initialBookPath,
                          const std::function<void()>& onGoBack)
      : ActivityWithSubactivity("Reader", renderer, inputManager),
        currentBookPath(std::move(initialBookPath)),
        onGoBack(onGoBack) {}

  void onEnter() override {
    ActivityWithSubactivity::onEnter();

    if (currentBookPath.empty()) {
      goToFileSelection();
    } else {
      openBook(currentBookPath);
    }
  }
};