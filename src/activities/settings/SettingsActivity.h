#pragma once
#include <functional>
#include <string>
#include <vector>
#include <cstring>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"
#include "Settings.h"

enum class SettingType { ENUM, ACTION };

// Structure to hold setting information
struct SettingInfo {
  const char* name;                        // Display name of the setting
  SettingType type;                        // Type of setting
  uint8_t Settings::* valuePtr;  // Pointer to member in Settings (for ENUM)
  std::vector<std::string> enumValues;

  // Static constructors
  static SettingInfo Enum(const char* name, uint8_t Settings::* ptr, std::vector<std::string> values) {
    return {name, SettingType::ENUM, ptr, std::move(values)};
  }

  static SettingInfo Action(const char* name) { return {name, SettingType::ACTION, nullptr}; }
};

// Define the static settings list
namespace {
constexpr int settingsCount = 4;
const SettingInfo settingsList[settingsCount] = {
    SettingInfo::Action("Continue Reading"),
    SettingInfo::Action("File Transfer"),
    SettingInfo::Enum("Time to Sleep", &Settings::sleepTimeout,
                      {"1 min", "5 min", "10 min", "15 min", "30 min"}),
    SettingInfo::Enum("Refresh Frequency", &Settings::refreshFrequency,
                      {"1 page", "5 pages", "10 pages", "15 pages", "30 pages"})};
}  // namespace

class SettingsActivity final : public SelectionActivity {
  const std::function<void()> onGoBackCallback;
  const std::function<void()> onContinueReading;
  const std::function<void()> onFileTransferOpen;

 protected:
  int getItemCount() const override {
    return settingsCount;
  }

  void renderItem(int index, int x, int y, bool isSelected) const override {
    if (index < 0 || index >= settingsCount) {
      return;
    }

    const auto& setting = settingsList[index];
    const auto pageWidth = renderer.getScreenWidth();

    // Draw setting name
    renderer.drawText(GfxRenderer::MEDIUM, x, y, setting.name, !isSelected);

    // Draw value based on setting type
    std::string valueText = "";
    if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
      const uint8_t value = SETTINGS.*(setting.valuePtr);
      valueText = setting.enumValues[value];
    }
    
    const auto width = renderer.getTextWidth(GfxRenderer::MEDIUM, valueText.c_str());
    renderer.drawText(GfxRenderer::MEDIUM, pageWidth - 20 - width, y, valueText.c_str(), !isSelected);
  }

  void onItemSelected(int index) override {
    // Validate index
    if (index < 0 || index >= settingsCount) {
      return;
    }

    const auto& setting = settingsList[index];

    if (setting.type == SettingType::ACTION) {
      // Handle action settings
      if (strcmp(setting.name, "Continue Reading") == 0) {
        onContinueReading();
      } else if (strcmp(setting.name, "File Transfer") == 0) {
        onFileTransferOpen();
      }
      return;
    }

    if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
      const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
      SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(setting.enumValues.size());
    }

    // Save settings when they change
    SETTINGS.saveToFile();
    
    // Trigger re-render to show updated value
    updateRequired = true;
  }

  void onBack() override {
    SETTINGS.saveToFile();
    onGoBackCallback();
  }

 public:
  explicit SettingsActivity(GfxRenderer& renderer, InputManager& inputManager,
                            const std::function<void()>& onGoBack, 
                            const std::function<void()>& onContinueReading,
                            const std::function<void()>& onFileTransferOpen)
      : SelectionActivity("Settings", "Options", renderer, inputManager),
        onGoBackCallback(onGoBack),
        onContinueReading(onContinueReading),
        onFileTransferOpen(onFileTransferOpen) {}
};