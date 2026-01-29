#include "Settings.h"

#include <SDCardManager.h>
#include <Serialization.h>

// Initialize the static instance
Settings Settings::instance;

namespace {
constexpr uint8_t SETTINGS_FILE_VERSION = 1;
// Updated from 4 to 3 since we removed sleepScreen
constexpr uint8_t SETTINGS_COUNT = 2;
constexpr char SETTINGS_FILE[] = "/.ereader/settings.bin";
}  // namespace

bool Settings::saveToFile() const {
  // Make sure the directory exists
  SdMan.mkdir("/.ereader");

  FsFile outputFile;
  if (!SdMan.openFileForWrite("CPS", SETTINGS_FILE, outputFile)) {
    return false;
  }

  serialization::writePod(outputFile, SETTINGS_FILE_VERSION);
  serialization::writePod(outputFile, SETTINGS_COUNT);
  serialization::writePod(outputFile, sleepTimeout);
  serialization::writePod(outputFile, refreshFrequency);
  outputFile.close();

  Serial.printf("[%lu] [CPS] Settings saved to file\n", millis());
  return true;
}

bool Settings::loadFromFile() {
  FsFile inputFile;
  if (!SdMan.openFileForRead("CPS", SETTINGS_FILE, inputFile)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version != SETTINGS_FILE_VERSION) {
    Serial.printf("[%lu] [CPS] Deserialization failed: Unknown version %u\n",
                  millis(), version);
    inputFile.close();
    return false;
  }

  uint8_t fileSettingsCount = 0;
  serialization::readPod(inputFile, fileSettingsCount);

  // load settings that exist (support older files with more or fewer fields)
  uint8_t settingsRead = 0;
  do {
    serialization::readPod(inputFile, sleepTimeout);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, refreshFrequency);
    if (++settingsRead >= fileSettingsCount) break;
  } while (false);

  inputFile.close();
  Serial.printf("[%lu] [CPS] Settings loaded from file\n", millis());
  return true;
}

unsigned long Settings::getSleepTimeoutMs() const {
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

int Settings::getRefreshFrequency() const {
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