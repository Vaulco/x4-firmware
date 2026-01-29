#pragma once
#include <cstdint>

class Settings {
 private:
  // Private constructor for singleton
  Settings() = default;

  // Static instance
  static Settings instance;

 public:
  // Delete copy constructor and assignment
  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;

  // Auto-sleep timeout options (in minutes)
  enum SLEEP_TIMEOUT { SLEEP_1_MIN = 0, SLEEP_5_MIN = 1, SLEEP_10_MIN = 2, SLEEP_15_MIN = 3, SLEEP_30_MIN = 4 };

  // E-ink refresh frequency (pages between full refreshes)
  enum REFRESH_FREQUENCY { REFRESH_1 = 0, REFRESH_5 = 1, REFRESH_10 = 2, REFRESH_15 = 3, REFRESH_30 = 4 };

  // Auto-sleep timeout setting (default 10 minutes)
  uint8_t sleepTimeout = SLEEP_10_MIN;
  // E-ink refresh frequency (default 15 pages)
  uint8_t refreshFrequency = REFRESH_15;

  ~Settings() = default;

  // Get singleton instance
  static Settings& getInstance() { return instance; }

  bool saveToFile() const;
  bool loadFromFile();

  unsigned long getSleepTimeoutMs() const;
  int getRefreshFrequency() const;
};

// Helper macro to access settings
#define SETTINGS Settings::getInstance()