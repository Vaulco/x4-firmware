#pragma once
#include <cstdint>
#include <string>
#include <SDCardManager.h>
#include <Serialization.h>

//Setting Definitions - Single Source of Truth
enum SettingId {
  SETTING_SLEEP_TIMEOUT = 0,
  SETTING_REFRESH_FREQUENCY,
  SETTING_COUNT  // Keep this last
};

struct EnumOption {
  const char* label;
  uint8_t value;
};

struct SettingMetadata {
  SettingId id;
  const char* name;
  const EnumOption* options;
  uint8_t optionCount;
  uint8_t defaultValue;
};

// Sleep timeout options
constexpr EnumOption SLEEP_OPTIONS[] = {
  {"1 min", 0},
  {"5 min", 1},
  {"10 min", 2},
  {"15 min", 3},
  {"30 min", 4}
};

// Refresh frequency options
constexpr EnumOption REFRESH_OPTIONS[] = {
  {"1 page", 0},
  {"5 pages", 1},
  {"10 pages", 2},
  {"15 pages", 3},
  {"30 pages", 4}
};

// Master settings list - add new settings here
constexpr SettingMetadata SETTING_DEFINITIONS[] = {
  {SETTING_SLEEP_TIMEOUT, "Time to Sleep", SLEEP_OPTIONS, 5, 2},       // default: 10 min
  {SETTING_REFRESH_FREQUENCY, "Refresh Frequency", REFRESH_OPTIONS, 5, 3}  // default: 15 pages
};

// Settings Class
class Settings {
private:
  Settings() {
    // Initialize with defaults
    for (const auto& def : SETTING_DEFINITIONS) {
      values[def.id] = def.defaultValue;
    }
  }
  
  static Settings instance;
  
  // Storage for all setting values
  uint8_t values[SETTING_COUNT];

public:
  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;
  
  std::string openBookPath;

  static Settings& getInstance() { return instance; }
  
  // Get/set setting values
  uint8_t getValue(SettingId id) const {
    return values[id];
  }
  
  void setValue(SettingId id, uint8_t value) {
    values[id] = value;
  }
  
  // Cycle to next option for a setting
  void cycleValue(SettingId id) {
    const auto& meta = SETTING_DEFINITIONS[id];
    values[id] = (values[id] + 1) % meta.optionCount;
  }
  
  // Get human-readable label for current value
  const char* getValueLabel(SettingId id) const {
    const auto& meta = SETTING_DEFINITIONS[id];
    const uint8_t value = values[id];
    
    for (uint8_t i = 0; i < meta.optionCount; i++) {
      if (meta.options[i].value == value) {
        return meta.options[i].label;
      }
    }
    return "Unknown";
  }
  
  // Convenience methods for computed values
  unsigned long getSleepTimeoutMs() const {
    const uint8_t value = values[SETTING_SLEEP_TIMEOUT];
    constexpr unsigned long TIMEOUTS[] = {
      1UL * 60 * 1000,   // 1 min
      5UL * 60 * 1000,   // 5 min
      10UL * 60 * 1000,  // 10 min
      15UL * 60 * 1000,  // 15 min
      30UL * 60 * 1000   // 30 min
    };
    return TIMEOUTS[value];
  }

  int getRefreshFrequency() const {
    const uint8_t value = values[SETTING_REFRESH_FREQUENCY];
    constexpr int FREQUENCIES[] = {1, 5, 10, 15, 30};
    return FREQUENCIES[value];
  }
  
  // File I/O
  bool saveToFile() const {
    constexpr char SETTINGS_FILE[] = "/.ereader/settings.bin";
    SdMan.mkdir("/.ereader");

    FsFile outputFile;
    if (!SdMan.openFileForWrite("CPS", SETTINGS_FILE, outputFile)) {
      return false;
    }

    // Write all setting values
    for (uint8_t i = 0; i < SETTING_COUNT; i++) {
      serialization::writePod(outputFile, values[i]);
    }
    
    serialization::writeString(outputFile, openBookPath);
    outputFile.close();

    Serial.printf("[%lu] [CPS] Settings saved to file\n", millis());
    return true;
  }

  bool loadFromFile() {
    constexpr char SETTINGS_FILE[] = "/.ereader/settings.bin";
    
    FsFile inputFile;
    if (!SdMan.openFileForRead("CPS", SETTINGS_FILE, inputFile)) {
      return false;
    }

    // Read all setting values
    for (uint8_t i = 0; i < SETTING_COUNT; i++) {
      serialization::readPod(inputFile, values[i]);
    }

    serialization::readString(inputFile, openBookPath);
    inputFile.close();
    
    Serial.printf("[%lu] [CPS] Settings loaded from file\n", millis());
    return true;
  }
};

inline Settings Settings::instance;

#define SETTINGS Settings::getInstance()