#include "Settings.h"

#include <SDCardManager.h>
#include <Serialization.h>

// Initialize the static instance
Settings Settings::instance;

namespace {
constexpr char SETTINGS_FILE[] = "/.ereader/settings.bin";
}

bool Settings::saveToFile() const {
  // Make sure the directory exists
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

bool Settings::loadFromFile() {
  FsFile inputFile;
  if (!SdMan.openFileForRead("CPS", SETTINGS_FILE, inputFile)) {
    return false;
  }

  // Read settings fields - if file is corrupted or incomplete, 
  // the fields will keep their default values
  serialization::readPod(inputFile, sleepTimeout);
  serialization::readPod(inputFile, refreshFrequency);
  serialization::readString(inputFile, openBookPath);

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