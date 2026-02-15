#include <Arduino.h>
#include <EInkDisplay.h>
#include <GfxRenderer.h>
#include <InputManager.h>
#include <SDCardManager.h>
#include <SPI.h>

#include "Battery.h"
#include "Settings.h"
#include "activities/network/CrossPointWebServerActivity.h"
#include "activities/reader/FileSelectionActivity.h"
#include "activities/reader/ReaderActivity.h"
#include "activities/settings/SettingsActivity.h"
#include "activities/util/FullScreenMessageActivity.h"

#define SPI_FQ 40000000
// Display SPI pins (custom pins for XteinkX4, not hardware SPI defaults)
#define EPD_SCLK 8   // SPI Clock
#define EPD_MOSI 10  // SPI MOSI (Master Out Slave In)
#define EPD_CS 21    // Chip Select
#define EPD_DC 4     // Data/Command
#define EPD_RST 5    // Reset
#define EPD_BUSY 6   // Busy

#define SD_SPI_MISO 7

EInkDisplay einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
InputManager inputManager;
GfxRenderer renderer(einkDisplay);
Activity* currentActivity = nullptr;

// Global BACK button long press tracking
constexpr unsigned long BACK_LONG_PRESS_MS = 1000;  // 1 second to go to settings
bool backLongPressConsumed = false;  // Flag to ignore BACK release after long press

void exitActivity() {
  if (currentActivity) {
    currentActivity->onExit();
    delete currentActivity;
    currentActivity = nullptr;
  }
}

void enterNewActivity(Activity* activity) {
  exitActivity();
  currentActivity = activity;
  currentActivity->onEnter();
}

void waitForPowerRelease() {
  inputManager.update();
  while (inputManager.isPressed(InputManager::Button::Power)) {
    delay(50);
    inputManager.update();
  }
}

// Display sleep screen and enter deep sleep mode
void enterDeepSleep() {
  exitActivity();
  
  // Display "SLEEPING" message
  const auto pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight / 2, "SLEEPING");
  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
  
  einkDisplay.deepSleep();
  Serial.printf("[%lu] [   ] Entering deep sleep.\n", millis());
  esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  waitForPowerRelease();
  // Enter Deep Sleep
  esp_deep_sleep_start();
}

// Forward declarations
void onGoToFileSelection();
void onGoToSettings();

void onGoToReader(const std::string& initialBookPath) {
  enterNewActivity(new ReaderActivity(renderer, inputManager, initialBookPath, onGoToSettings));
}

void onContinueReading() { onGoToReader(SETTINGS.openBookPath); }

void onGoToFileTransfer() {
  enterNewActivity(new CrossPointWebServerActivity(renderer, inputManager, onGoToFileSelection));
}

void onGoToSettings() {
  enterNewActivity(
      new SettingsActivity(renderer, inputManager, onGoToFileSelection, onContinueReading, onGoToFileTransfer));
}

void onGoToFileSelection() {
  enterNewActivity(new FileSelectionActivity(
      renderer, inputManager,
      [](const std::string& path) { onGoToReader(path); },
      onGoToSettings));
}

void setupDisplayAndFonts() {
  einkDisplay.begin();
  Serial.printf("[%lu] [   ] Display initialized\n", millis());
  Serial.printf("[%lu] [   ] Fonts setup\n", millis());
}

void setup() {
  Serial.begin(115200);

  inputManager.begin();
  // Initialize pins
  pinMode(BAT_GPIO0, INPUT);

  // Initialize SPI with custom pins
  SPI.begin(EPD_SCLK, SD_SPI_MISO, EPD_MOSI, EPD_CS);

  // SD Card Initialization
  if (!SdMan.begin()) {
    Serial.printf("[%lu] [   ] SD card initialization failed\n", millis());
    setupDisplayAndFonts();
    enterNewActivity(new FullScreenMessageActivity(renderer, inputManager, "SD card error"));
    return;
  }

  SETTINGS.loadFromFile();

  // First serial output only here to avoid timing inconsistencies for power button press duration verification
  Serial.printf("[%lu] [   ] Starting CrossPoint\n", millis());

  setupDisplayAndFonts();

  // Simplified boot logic - no defensive clearing
  if (SETTINGS.openBookPath.empty()) {
    onGoToFileSelection();
  } else {
    onGoToReader(SETTINGS.openBookPath);
  }
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  inputManager.update();

  if (Serial && millis() - lastMemPrint >= 10000) {
    Serial.printf("[%lu] [MEM] Free: %d bytes, Total: %d bytes, Min Free: %d bytes\n", 
                  millis(), ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getMinFreeHeap());
    lastMemPrint = millis();
  }

  // Check for any user activity
  static unsigned long lastActivityTime = millis();
  if (inputManager.wasAnyPressed() || inputManager.wasAnyReleased() ||
      (currentActivity && currentActivity->preventAutoSleep())) {
    lastActivityTime = millis();
  }

  // Auto-sleep check
  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (millis() - lastActivityTime >= sleepTimeoutMs) {
    Serial.printf("[%lu] [SLP] Auto-sleep triggered after %lu ms of inactivity\n", 
                  millis(), sleepTimeoutMs);
    enterDeepSleep();
    return;
  }

  // Power button instant sleep
  if (inputManager.wasPressed(InputManager::Button::Power)) {
    enterDeepSleep();
    return;
  }

  // GLOBAL: Back button long press to Settings
  if (inputManager.isPressed(InputManager::Button::Back) && 
      inputManager.getHeldTime() >= BACK_LONG_PRESS_MS && 
      !backLongPressConsumed) {
    
    Serial.printf("[%lu] [MAIN] BACK button held for %lu ms - navigating to Settings\n", 
                  millis(), inputManager.getHeldTime());
    backLongPressConsumed = true;
    onGoToSettings();
    return;
  }

  // Clear consumed flag when released
  if (backLongPressConsumed && inputManager.wasReleased(InputManager::Button::Back)) {
    Serial.printf("[%lu] [MAIN] BACK button released after long press - ignoring release event\n", 
                  millis());
    backLongPressConsumed = false;
    return;
  }

  // Run activity loop and measure duration
  unsigned long activityDuration = 0;
  if (currentActivity) {
    const unsigned long activityStartTime = millis();
    currentActivity->loop();
    activityDuration = millis() - activityStartTime;
  }

  // Performance tracking
  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50 && currentActivity) {
      Serial.printf("[%lu] [LOOP] New max loop duration: %lu ms (activity: %lu ms)\n", 
                    millis(), maxLoopDuration, activityDuration);
    }
  }

  // Loop delay
  if (currentActivity && currentActivity->skipLoopDelay()) {
    yield();
  } else {
    delay(10);
  }
}