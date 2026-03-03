#pragma once
#include <functional>
#include <GfxRenderer.h>

#include "activities/util/SelectionActivity.h"
#include "Settings.h"

// Action IDs for non-setting menu items
enum SettingActionId {
  ACTION_CONTINUE_READING = SETTING_COUNT,  // Start after settings
  ACTION_TOTAL_COUNT
};

class SettingsActivity final : public SelectionActivity {
  const std::function<void()> onGoBackCallback;
  const std::function<void()> onContinueReading;

protected:
  int getItemCount() const override { 
    return ACTION_TOTAL_COUNT;  // Settings + actions
  }

  void renderItem(int index, int x, int y, bool isSelected) const override {
    if (index < 0 || index >= ACTION_TOTAL_COUNT) return;

    const auto pageWidth = renderer.getScreenWidth();

    // Handle actions (non-settings)
    if (index >= SETTING_COUNT) {
      const char* actionNames[] = {
        "Continue Reading",
      };
      renderer.drawText(GfxRenderer::MEDIUM, x, y, 
                       actionNames[index - SETTING_COUNT], !isSelected);
      return;
    }

    // Handle settings - auto-generate from metadata
    const auto& meta = SETTING_DEFINITIONS[index];
    renderer.drawText(GfxRenderer::MEDIUM, x, y, meta.name, !isSelected);

    // Show current value on right side
    const char* valueLabel = SETTINGS.getValueLabel(static_cast<SettingId>(index));
    const auto width = renderer.getTextWidth(GfxRenderer::MEDIUM, valueLabel);
    renderer.drawText(GfxRenderer::MEDIUM, pageWidth - 20 - width, y, 
                     valueLabel, !isSelected);
  }

  void onItemSelected(int index) override {
    if (index < 0 || index >= ACTION_TOTAL_COUNT) return;

    // Handle actions
    if (index == ACTION_CONTINUE_READING) {
      onContinueReading();
      return;
    }

    // Handle settings - cycle to next value
    SETTINGS.cycleValue(static_cast<SettingId>(index));
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
                           const std::function<void()>& onContinueReading)
      : SelectionActivity("Settings", "Options", renderer, inputManager),
        onGoBackCallback(onGoBack),
        onContinueReading(onContinueReading) {}
};