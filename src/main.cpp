#include <Arduino.h>
#include <EInkDisplay.h>
#include <GfxRenderer.h>
#include <InputManager.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <builtinFonts/all.h>

#include "Battery.h"
#include "Settings.h"
#include "State.h"
#include "MappedInputManager.h"
#include "activities/boot_sleep/SleepActivity.h"
#include "activities/network/CrossPointWebServerActivity.h"
#include "activities/reader/FileSelectionActivity.h"
#include "activities/reader/ReaderActivity.h"
#include "activities/settings/SettingsActivity.h"
#include "activities/util/FullScreenMessageActivity.h"
#include "fontIds.h"

#define SPI_FQ 40000000
// Display SPI pins (custom pins for XteinkX4, not hardware SPI defaults)
#define EPD_SCLK 8   // SPI Clock
#define EPD_MOSI 10  // SPI MOSI (Master Out Slave In)
#define EPD_CS 21    // Chip Select
#define EPD_DC 4     // Data/Command
#define EPD_RST 5    // Reset
#define EPD_BUSY 6   // Busy

#define UART0_RXD 20  // Used for USB connection detection

#define SD_SPI_MISO 7

EInkDisplay einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
InputManager inputManager;
MappedInputManager mappedInputManager(inputManager);
GfxRenderer renderer(einkDisplay);
Activity* currentActivity;

EpdFont cmu8RegularFont(&cmu_8_regular);
EpdFont cmu8BoldFont(&cmu_8_bold);
EpdFont cmu8ItalicFont(&cmu_8_italic);
EpdFontFamily cmu8FontFamily(&cmu8RegularFont, &cmu8BoldFont, &cmu8ItalicFont);

EpdFont cmu10RegularFont(&cmu_10_regular);
EpdFont cmu10BoldFont(&cmu_10_bold);
EpdFont cmu10ItalicFont(&cmu_10_italic);
EpdFontFamily cmu10FontFamily(&cmu10RegularFont, &cmu10BoldFont, &cmu10ItalicFont);

EpdFont cmu12RegularFont(&cmu_12_regular);
EpdFont cmu12BoldFont(&cmu_12_bold);
EpdFont cmu12ItalicFont(&cmu_12_italic);
EpdFontFamily cmu12FontFamily(&cmu12RegularFont, &cmu12BoldFont, &cmu12ItalicFont);

EpdFont cmu14RegularFont(&cmu_14_regular);
EpdFont cmu14BoldFont(&cmu_14_bold);
EpdFont cmu14ItalicFont(&cmu_14_italic);
EpdFontFamily cmu14FontFamily(&cmu14RegularFont, &cmu14BoldFont, &cmu14ItalicFont);

// measurement of power button press duration calibration value
unsigned long t1 = 0;
unsigned long t2 = 0;

void exitActivity() {
  if (currentActivity) {
    currentActivity->onExit();
    delete currentActivity;
    currentActivity = nullptr;
  }
}

void enterNewActivity(Activity* activity) {
  currentActivity = activity;
  currentActivity->onEnter();
}

void waitForPowerRelease() {
  inputManager.update();
  while (inputManager.isPressed(InputManager::BTN_POWER)) {
    delay(50);
    inputManager.update();
  }
}

// Enter deep sleep mode
void enterDeepSleep() {
  exitActivity();
  enterNewActivity(new SleepActivity(renderer, mappedInputManager));

  einkDisplay.deepSleep();
  Serial.printf("[%lu] [   ] Power button press calibration value: %lu ms\n", millis(), t2 - t1);
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

void onGoToReader(const std::string& initialEpubPath) {
  exitActivity();
  enterNewActivity(new ReaderActivity(renderer, mappedInputManager, initialEpubPath, onGoToSettings));
}

void onContinueReading() { onGoToReader(APP_STATE.openEpubPath); }

void onGoToFileTransfer() {
  exitActivity();
  enterNewActivity(new CrossPointWebServerActivity(renderer, mappedInputManager, onGoToFileSelection));
}

void onGoToSettings() {
  exitActivity();
  enterNewActivity(
      new SettingsActivity(renderer, mappedInputManager, onGoToFileSelection, onContinueReading, onGoToFileTransfer));
}

void onGoToFileSelection() {
  exitActivity();
  enterNewActivity(new FileSelectionActivity(
      renderer, mappedInputManager,
      [](const std::string& path) { onGoToReader(path); },
      onGoToSettings));
}

void setupDisplayAndFonts() {
  einkDisplay.begin();
  Serial.printf("[%lu] [   ] Display initialized\n", millis());
  renderer.insertFont(CMU_8_FONT_ID, cmu8FontFamily);
  renderer.insertFont(CMU_10_FONT_ID, cmu10FontFamily);
  renderer.insertFont(CMU_12_FONT_ID, cmu12FontFamily);
  renderer.insertFont(CMU_14_FONT_ID, cmu14FontFamily);
  Serial.printf("[%lu] [   ] Fonts setup\n", millis());
}

void setup() {
  t1 = millis();

  // Only start serial if USB connected
  pinMode(UART0_RXD, INPUT);
  if (digitalRead(UART0_RXD) == HIGH) {
    Serial.begin(115200);
  }

  inputManager.begin();
  // Initialize pins
  pinMode(BAT_GPIO0, INPUT);

  // Initialize SPI with custom pins
  SPI.begin(EPD_SCLK, SD_SPI_MISO, EPD_MOSI, EPD_CS);

  // SD Card Initialization
  // We need 6 open files concurrently when parsing a new chapter
  if (!SdMan.begin()) {
    Serial.printf("[%lu] [   ] SD card initialization failed\n", millis());
    setupDisplayAndFonts();
    exitActivity();
    enterNewActivity(new FullScreenMessageActivity(renderer, mappedInputManager, "SD card error", EpdFontFamily::BOLD));
    return;
  }

  SETTINGS.loadFromFile();

  // First serial output only here to avoid timing inconsistencies for power button press duration verification
  Serial.printf("[%lu] [   ] Starting CrossPoint\n", millis());

  setupDisplayAndFonts();

  APP_STATE.loadFromFile();
  if (APP_STATE.openEpubPath.empty()) {
    onGoToFileSelection();
  } else {
    // Clear app state to avoid getting into a boot loop if the epub doesn't load
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.saveToFile();
    onGoToReader(path);
  }

  // Ensure we're not still holding the power button before leaving setup
  waitForPowerRelease();
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  inputManager.update();

  if (Serial && millis() - lastMemPrint >= 10000) {
    Serial.printf("[%lu] [MEM] Free: %d bytes, Total: %d bytes, Min Free: %d bytes\n", millis(), ESP.getFreeHeap(),
                  ESP.getHeapSize(), ESP.getMinFreeHeap());
    lastMemPrint = millis();
  }

  // Check for any user activity (button press or release) or active background work
  static unsigned long lastActivityTime = millis();
  if (inputManager.wasAnyPressed() || inputManager.wasAnyReleased() ||
      (currentActivity && currentActivity->preventAutoSleep())) {
    lastActivityTime = millis();  // Reset inactivity timer
  }

  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (millis() - lastActivityTime >= sleepTimeoutMs) {
    Serial.printf("[%lu] [SLP] Auto-sleep triggered after %lu ms of inactivity\n", millis(), sleepTimeoutMs);
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  // Changed from long press to instant press for sleep
  if (inputManager.wasPressed(InputManager::BTN_POWER)) {
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  const unsigned long activityStartTime = millis();
  if (currentActivity) {
    currentActivity->loop();
  }
  const unsigned long activityDuration = millis() - activityStartTime;

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      Serial.printf("[%lu] [LOOP] New max loop duration: %lu ms (activity: %lu ms)\n", millis(), maxLoopDuration,
                    activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // When an activity requests skip loop delay (e.g., webserver running), use yield() for faster response
  // Otherwise, use longer delay to save power
  if (currentActivity && currentActivity->skipLoopDelay()) {
    yield();  // Give FreeRTOS a chance to run tasks, but return immediately
  } else {
    delay(10);  // Normal delay when no activity requires fast response
  }
}