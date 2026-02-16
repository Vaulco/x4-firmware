#pragma once
#include <cstdint>
#include <string>
#include <SDCardManager.h>
#include <Serialization.h>

class Settings {
 private:
  Settings() = default;
  static Settings instance;

 public:
  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;

  enum SLEEP_TIMEOUT { SLEEP_1_MIN = 0, SLEEP_5_MIN = 1, SLEEP_10_MIN = 2, SLEEP_15_MIN = 3, SLEEP_30_MIN = 4 };
  enum REFRESH_FREQUENCY { REFRESH_1 = 0, REFRESH_5 = 1, REFRESH_10 = 2, REFRESH_15 = 3, REFRESH_30 = 4 };

  uint8_t sleepTimeout = SLEEP_10_MIN;
  uint8_t refreshFrequency = REFRESH_15;
  
  std::string openBookPath;

  ~Settings() = default;

  static Settings& getInstance() { return instance; }

  bool saveToFile() const {
    constexpr char SETTINGS_FILE[] = "/.ereader/settings.bin";
    
    SdMan.mkdir("/.ereader");

    FsFile outputFile;
    if (!SdMan.openFileForWrite("CPS", SETTINGS_FILE, outputFile)) {
      return false;
    }

    serialization::writePod(outputFile, sleepTimeout);
    serialization::writePod(outputFile, refreshFrequency);
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

    serialization::readPod(inputFile, sleepTimeout);
    serialization::readPod(inputFile, refreshFrequency);
    serialization::readString(inputFile, openBookPath);

    inputFile.close();
    Serial.printf("[%lu] [CPS] Settings loaded from file\n", millis());
    return true;
  }

  unsigned long getSleepTimeoutMs() const {
    switch (sleepTimeout) {
      case SLEEP_1_MIN:
        return 1UL * 60 * 1000;
      case SLEEP_5_MIN:
        return 5UL * 60 * 1000;
      case SLEEP_10_MIN:
      default:
        return 10UL * 60 * 1000;
      case SLEEP_15_MIN:
        return 15UL * 60 * 1000;
      case SLEEP_30_MIN:
        return 30UL * 60 * 1000;
    }
  }

  int getRefreshFrequency() const {
    switch (refreshFrequency) {
      case REFRESH_1:
        return 1;
      case REFRESH_5:
        return 5;
      case REFRESH_10:
        return 10;
      case REFRESH_15:
      default:
        return 15;
      case REFRESH_30:
        return 30;
    }
  }
};

inline Settings Settings::instance;

#define SETTINGS Settings::getInstance()