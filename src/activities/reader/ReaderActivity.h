#pragma once
#include <memory>
#include <Xtc.h>

#include "activities/ActivityWithSubactivity.h"
#include "FileSelection.h"
#include "XtcReaderActivity.h"

class ReaderActivity final : public ActivityWithSubactivity {
  std::string currentBookPath;  // Track current book path for navigation
  const std::function<void()> onGoBack;
  const std::function<void()> onGoToBible;

  static std::unique_ptr<Xtc> loadXtc(const std::string& path) {
      auto xtc = std::make_unique<Xtc>(path, "/.ereader");
      if (!xtc->load()) return nullptr;
      return xtc;
  }

  void showErrorAndReturnToFileSelection() {
    const auto pageHeight = renderer.getScreenHeight();
    renderer.clearScreen();
    renderer.drawCenteredText(GfxRenderer::MEDIUM, (pageHeight - renderer.getLineHeight(GfxRenderer::MEDIUM)) / 2,
                              "Failed to load XTC", true);
    renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
    delay(2000);
    goToFileSelection();
  }

  static constexpr const char* BIBLE_NKJV_FOLDER = "/\xD0\x91\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD1\x8F/NKJV"; // "/Библия/NKJV"
  static constexpr const char* BIBLE_RSV_FOLDER = "/\xD0\x91\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD1\x8F/RSV";   // "/Библия/RSV"

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

    // Books opened from the Bible grid (NKJV/RSV) should return to the
    // grid on exit, not the plain file list.
    if (folder == BIBLE_NKJV_FOLDER || folder == BIBLE_RSV_FOLDER) {
      onGoToBible();
      return;
    }
    
    enterNewActivity(new FileSelection(
        renderer, inputManager, 
        [this](const std::string& path) { openBook(path); }, 
        onGoBack, 
        onGoToBible,
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
                          const std::function<void()>& onGoBack,
                          const std::function<void()>& onGoToBible)
      : ActivityWithSubactivity("Reader", renderer, inputManager),
        currentBookPath(std::move(initialBookPath)),
        onGoBack(onGoBack),
        onGoToBible(onGoToBible) {}

  void onEnter() override {
    ActivityWithSubactivity::onEnter();

    if (currentBookPath.empty()) {
      goToFileSelection();
    } else {
      openBook(currentBookPath);
    }
  }
};