#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class FileSelectionActivity final : public Activity {
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  std::string basepath = "/";
  std::vector<std::string> files;
  int selectorIndex = 0;
  bool updateRequired = false;
  unsigned long activityStartTime = 0;  // For button debouncing
  const std::function<void(const std::string&)> onSelect;
  const std::function<void()> onGoToSettings;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void render() const;
  void loadFiles();

 public:
  explicit FileSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::function<void(const std::string&)>& onSelect,
                                 const std::function<void()>& onGoToSettings, std::string initialPath = "/")
      : Activity("FileSelection", renderer, mappedInput),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)),
        onSelect(onSelect),
        onGoToSettings(onGoToSettings) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};