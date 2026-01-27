#pragma once
#include <cstdint>
#include <iosfwd>

class CrossPointSettings {
 private:
  // Private constructor for singleton
  CrossPointSettings() = default;

  // Static instance
  static CrossPointSettings instance;

 public:
  // Delete copy constructor and assignment
  CrossPointSettings(const CrossPointSettings&) = delete;
  CrossPointSettings& operator=(const CrossPointSettings&) = delete;

  // Should match with SettingsActivity text
  enum SLEEP_SCREEN_MODE { DARK = 0, LIGHT = 1, CUSTOM = 2, COVER = 3, BLANK = 4 };

  // Front button layout options
  // Default: Back, Confirm, Left, Right
  // Swapped: Left, Right, Back, Confirm
  enum FRONT_BUTTON_LAYOUT { BACK_CONFIRM_LEFT_RIGHT = 0, LEFT_RIGHT_BACK_CONFIRM = 1, LEFT_BACK_CONFIRM_RIGHT = 2 };

  // Auto-sleep timeout options (in minutes)
  enum SLEEP_TIMEOUT { SLEEP_1_MIN = 0, SLEEP_5_MIN = 1, SLEEP_10_MIN = 2, SLEEP_15_MIN = 3, SLEEP_30_MIN = 4 };

  // E-ink refresh frequency (pages between full refreshes)
  enum REFRESH_FREQUENCY { REFRESH_1 = 0, REFRESH_5 = 1, REFRESH_10 = 2, REFRESH_15 = 3, REFRESH_30 = 4 };

  // Sleep screen settings
  uint8_t sleepScreen = DARK;
  // Duration of the power button press
  uint8_t shortPwrBtn = 0;
  // Button layouts
  uint8_t frontButtonLayout = BACK_CONFIRM_LEFT_RIGHT;

  // Auto-sleep timeout setting (default 10 minutes)
  uint8_t sleepTimeout = SLEEP_10_MIN;
  // E-ink refresh frequency (default 15 pages)
  uint8_t refreshFrequency = REFRESH_15;

  ~CrossPointSettings() = default;

  // Get singleton instance
  static CrossPointSettings& getInstance() { return instance; }

  uint16_t getPowerButtonDuration() const { return shortPwrBtn ? 10 : 400; }

  bool saveToFile() const;
  bool loadFromFile();

  unsigned long getSleepTimeoutMs() const;
  int getRefreshFrequency() const;
};

// Helper macro to access settings
#define SETTINGS CrossPointSettings::getInstance()