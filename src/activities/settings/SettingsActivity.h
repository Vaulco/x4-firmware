#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "activities/ActivityWithSubactivity.h"

class CrossPointSettings;

enum class SettingType { ENUM, ACTION };

// Structure to hold setting information
struct SettingInfo {
  const char* name;                        // Display name of the setting
  SettingType type;                        // Type of setting
  uint8_t CrossPointSettings::* valuePtr;  // Pointer to member in CrossPointSettings (for ENUM)
  std::vector<std::string> enumValues;

  // Static constructors
  static SettingInfo Enum(const char* name, uint8_t CrossPointSettings::* ptr, std::vector<std::string> values) {
    return {name, SettingType::ENUM, ptr, std::move(values)};
  }

  static SettingInfo Action(const char* name) { return {name, SettingType::ACTION, nullptr}; }
};

class SettingsActivity final : public ActivityWithSubactivity {
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  bool updateRequired = false;
  int selectedSettingIndex = 0;  // Currently selected setting
  unsigned long activityStartTime = 0;  // For button debouncing
  const std::function<void()> onGoBack;
  const std::function<void()> onContinueReading;
  const std::function<void()> onFileTransferOpen;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void render() const;
  void handleSettingAction();

 public:
  explicit SettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                            const std::function<void()>& onGoBack, 
                            const std::function<void()>& onContinueReading,
                            const std::function<void()>& onFileTransferOpen)
      : ActivityWithSubactivity("Settings", renderer, mappedInput),
        onGoBack(onGoBack),
        onContinueReading(onContinueReading),
        onFileTransferOpen(onFileTransferOpen) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};