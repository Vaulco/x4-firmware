#pragma once
#include <functional>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"
#include "Settings.h"

enum class SettingType { ENUM, ACTION };

struct SettingInfo {
  const char* name;
  SettingType type;
  uint8_t Settings::* valuePtr;
  const char* const* enumValues;
  uint8_t enumCount;

  static SettingInfo Enum(const char* name, uint8_t Settings::* ptr, 
                         const char* const* values, uint8_t count) {
    return {name, SettingType::ENUM, ptr, values, count};
  }

  static SettingInfo Action(const char* name) { 
    return {name, SettingType::ACTION, nullptr, nullptr, 0}; 
  }
};

namespace {
constexpr int settingsCount = 4;

const char* const sleepTimeoutValues[] = {"1 min", "5 min", "10 min", "15 min", "30 min"};
const char* const refreshFrequencyValues[] = {"1 page", "5 pages", "10 pages", "15 pages", "30 pages"};

const SettingInfo settingsList[settingsCount] = {
    SettingInfo::Action("Continue Reading"),
    SettingInfo::Action("File Transfer"),
    SettingInfo::Enum("Time to Sleep", &Settings::sleepTimeout, 
                      sleepTimeoutValues, 5),
    SettingInfo::Enum("Refresh Frequency", &Settings::refreshFrequency,
                      refreshFrequencyValues, 5)
};
}

class SettingsActivity final : public SelectionActivity {
  const std::function<void()> onGoBackCallback;
  const std::function<void()> onContinueReading;
  const std::function<void()> onFileTransferOpen;

 protected:
  int getItemCount() const override { return settingsCount; }

  void renderItem(int index, int x, int y, bool isSelected) const override {
    if (index < 0 || index >= settingsCount) return;

    const auto& setting = settingsList[index];
    const auto pageWidth = renderer.getScreenWidth();

    renderer.drawText(GfxRenderer::MEDIUM, x, y, setting.name, !isSelected);

    std::string valueText = "";
    if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
      const uint8_t value = SETTINGS.*(setting.valuePtr);
      valueText = setting.enumValues[value];
    }
    
    const auto width = renderer.getTextWidth(GfxRenderer::MEDIUM, valueText.c_str());
    renderer.drawText(GfxRenderer::MEDIUM, pageWidth - 20 - width, y, valueText.c_str(), !isSelected);
  }

  void onItemSelected(int index) override {
    if (index < 0 || index >= settingsCount) return;

    const auto& setting = settingsList[index];

    if (setting.type == SettingType::ACTION) {
      // Index-based comparison instead of strcmp
      if (index == 0) {  // Continue Reading
        onContinueReading();
      } else if (index == 1) {  // File Transfer
        onFileTransferOpen();
      }
      return;
    }

    if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
      const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
      SETTINGS.*(setting.valuePtr) = (currentValue + 1) % setting.enumCount;
    }

    SETTINGS.saveToFile();
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