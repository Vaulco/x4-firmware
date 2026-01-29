#include "SettingsActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include "Settings.h"
#include "State.h"
#include "MappedInputManager.h"
#include "Battery.h"
#include "fontIds.h"

// Define the static settings list - Continue Reading is now the first item
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

void SettingsActivity::taskTrampoline(void* param) {
  auto* self = static_cast<SettingsActivity*>(param);
  self->displayTaskLoop();
}

void SettingsActivity::onEnter() {
  Activity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();

  // Reset selection to first item
  selectedSettingIndex = 0;

  // Record entry time for button debouncing
  activityStartTime = millis();

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&SettingsActivity::taskTrampoline, "SettingsActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void SettingsActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void SettingsActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  // Debounce button presses for first 300ms after entering activity
  const unsigned long timeSinceStart = millis() - activityStartTime;
  constexpr unsigned long INPUT_DEBOUNCE_MS = 300;

  // Handle actions with early return
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSettingAction();
    updateRequired = true;
    return;
  }

  if (timeSinceStart > INPUT_DEBOUNCE_MS && mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    onGoBack();
    return;
  }

  // Handle navigation
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    // Move selection up (with wrap-around)
    selectedSettingIndex = (selectedSettingIndex > 0) ? (selectedSettingIndex - 1) : (settingsCount - 1);
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
             mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    // Move selection down (with wrap around)
    selectedSettingIndex = (selectedSettingIndex < settingsCount - 1) ? (selectedSettingIndex + 1) : 0;
    updateRequired = true;
  }
}

void SettingsActivity::handleSettingAction() {
  // Validate index
  if (selectedSettingIndex < 0 || selectedSettingIndex >= settingsCount) {
    return;
  }

  const auto& setting = settingsList[selectedSettingIndex];

  if (setting.type == SettingType::ACTION) {
    // Handle action settings
    if (strcmp(setting.name, "Continue Reading") == 0) {
      // Check if we have a book to continue reading
      if (!APP_STATE.openEpubPath.empty() && SdMan.exists(APP_STATE.openEpubPath.c_str())) {
        if (onContinueReading) {
          onContinueReading();
        }
      } else {
        // No book available - could show a message or do nothing
        Serial.printf("[%lu] [SETTINGS] No book available to continue reading\n", millis());
      }
    } else if (strcmp(setting.name, "File Transfer") == 0) {
      if (onFileTransferOpen) {
        onFileTransferOpen();
      }
    }
    return;
  }

  if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(setting.enumValues.size());
  }

  // Save settings when they change
  SETTINGS.saveToFile();
}

void SettingsActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired && !subActivity) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void SettingsActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // Draw header
  renderer.drawCenteredText(CMU_12_FONT_ID, 15, "Options", true, EpdFontFamily::REGULAR);

  // Draw selection
  renderer.fillRect(0, 60 + selectedSettingIndex * 30 - 2, pageWidth - 1, 30);

  // Draw all settings
  for (int i = 0; i < settingsCount; i++) {
    const int settingY = 60 + i * 30;  // 30 pixels between settings

    // Draw setting name
    renderer.drawText(CMU_10_FONT_ID, 20, settingY, settingsList[i].name, i != selectedSettingIndex);

    // Draw value based on setting type
    std::string valueText = "";
    if (settingsList[i].type == SettingType::ENUM && settingsList[i].valuePtr != nullptr) {
      const uint8_t value = SETTINGS.*(settingsList[i].valuePtr);
      valueText = settingsList[i].enumValues[value];
    }
    
    const auto width = renderer.getTextWidth(CMU_10_FONT_ID, valueText.c_str());
    renderer.drawText(CMU_10_FONT_ID, pageWidth - 20 - width, settingY, valueText.c_str(), i != selectedSettingIndex);
  }

  // Draw battery indicator centered at bottom
  const int batteryWidth = 70;  // Approximate width for battery icon + percentage text
  const int batteryX = (pageWidth - batteryWidth) / 2;
  battery.draw(renderer, batteryX, pageHeight - 30);

  // Always use standard refresh for settings screen
  renderer.displayBuffer();
}