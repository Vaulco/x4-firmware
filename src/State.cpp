#include "State.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <Serialization.h>

namespace {
constexpr uint8_t STATE_FILE_VERSION = 1;
constexpr char STATE_FILE[] = "/.ereader/state.bin";
}  // namespace

State State::instance;

bool State::saveToFile() const {
  FsFile outputFile;
  if (!SdMan.openFileForWrite("CPS", STATE_FILE, outputFile)) {
    return false;
  }

  serialization::writePod(outputFile, STATE_FILE_VERSION);
  serialization::writeString(outputFile, openEpubPath);
  outputFile.close();
  return true;
}

bool State::loadFromFile() {
  FsFile inputFile;
  if (!SdMan.openFileForRead("CPS", STATE_FILE, inputFile)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version != STATE_FILE_VERSION) {
    Serial.printf("[%lu] [CPS] Deserialization failed: Unknown version %u\n", millis(), version);
    inputFile.close();
    return false;
  }

  serialization::readString(inputFile, openEpubPath);

  inputFile.close();
  return true;
}
