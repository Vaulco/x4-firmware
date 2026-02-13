#pragma once

#include <Xtc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "activities/ActivityWithSubactivity.h"

class XtcReaderActivity final : public ActivityWithSubactivity {
  std::shared_ptr<Xtc> xtc;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  uint32_t currentPage = 0;
  int pagesUntilFullRefresh = 0;
  bool updateRequired = false;
  const std::function<void()> onGoBackToFileSelection;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderPage();
  void saveProgress() const;
  void loadProgress();

 public:
  explicit XtcReaderActivity(GfxRenderer& renderer, InputManager& inputManager, std::unique_ptr<Xtc> xtc,
                             const std::function<void()>& onGoBackToFileSelection)
      : ActivityWithSubactivity("XtcReader", renderer, inputManager),
        xtc(std::move(xtc)),
        onGoBackToFileSelection(onGoBackToFileSelection) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};