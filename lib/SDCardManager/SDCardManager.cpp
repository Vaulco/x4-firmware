#include "SDCardManager.h"

namespace {
constexpr uint8_t SD_CS = 12;
constexpr uint32_t SPI_FQ = 40000000;
}

SDCardManager SDCardManager::instance;

SDCardManager::SDCardManager() : sd() {}

bool SDCardManager::begin() {
  if (!sd.begin(SD_CS, SPI_FQ)) {
    Serial.printf("[%lu] [SD] SD card not detected\n", millis());
    initialized = false;
  } else {
    Serial.printf("[%lu] [SD] SD card detected\n", millis());
    initialized = true;
  }
  return initialized;
}

bool SDCardManager::ready() const {
  return initialized;
}

std::vector<String> SDCardManager::listFiles(const char* path, const int maxFiles) {
  std::vector<String> ret;
  if (!initialized) {
    Serial.printf("[%lu] [SD] not initialized, returning empty list\n", millis());
    return ret;
  }

  auto root = sd.open(path);
  if (!root) {
    Serial.printf("[%lu] [SD] Failed to open directory: %s\n", millis(), path);
    return ret;
  }
  if (!root.isDirectory()) {
    Serial.printf("[%lu] [SD] Path is not a directory: %s\n", millis(), path);
    root.close();
    return ret;
  }

  int count = 0;
  char name[128];
  for (auto f = root.openNextFile(); f && count < maxFiles; f = root.openNextFile()) {
    if (f.isDirectory()) {
      f.close();
      continue;
    }
    f.getName(name, sizeof(name));
    ret.emplace_back(name);
    f.close();
    count++;
  }
  root.close();
  return ret;
}

bool SDCardManager::ensureDirectoryExists(const char* path) {
  if (!initialized) {
    Serial.printf("[%lu] [SD] not initialized; cannot create directory\n", millis());
    return false;
  }

  // Check if directory already exists
  if (sd.exists(path)) {
    FsFile dir = sd.open(path);
    if (dir && dir.isDirectory()) {
      dir.close();
      Serial.printf("[%lu] [SD] Directory already exists: %s\n", millis(), path);
      return true;
    }
    dir.close();
  }

  // Create the directory
  if (sd.mkdir(path)) {
    Serial.printf("[%lu] [SD] Created directory: %s\n", millis(), path);
    return true;
  } else {
    Serial.printf("[%lu] [SD] Failed to create directory: %s\n", millis(), path);
    return false;
  }
}

bool SDCardManager::openFileForRead(const char* moduleName, const char* path, FsFile& file) {
  if (!sd.exists(path)) {
    Serial.printf("[%lu] [%s] File does not exist: %s\n", millis(), moduleName, path);
    return false;
  }

  file = sd.open(path, O_RDONLY);
  if (!file) {
    Serial.printf("[%lu] [%s] Failed to open file for reading: %s\n", millis(), moduleName, path);
    return false;
  }
  return true;
}

bool SDCardManager::openFileForRead(const char* moduleName, const std::string& path, FsFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForRead(const char* moduleName, const String& path, FsFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForWrite(const char* moduleName, const char* path, FsFile& file) {
  file = sd.open(path, O_RDWR | O_CREAT | O_TRUNC);
  if (!file) {
    Serial.printf("[%lu] [%s] Failed to open file for writing: %s\n", millis(), moduleName, path);
    return false;
  }
  return true;
}

bool SDCardManager::openFileForWrite(const char* moduleName, const std::string& path, FsFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForWrite(const char* moduleName, const String& path, FsFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool SDCardManager::removeDir(const char* path) {
  if (!initialized) {
    return false;
  }

  auto dir = sd.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  auto file = dir.openNextFile();
  char name[128];
  while (file) {
    String filePath = path;
    if (!filePath.endsWith("/")) {
      filePath += "/";
    }
    file.getName(name, sizeof(name));
    filePath += name;

    if (file.isDirectory()) {
      if (!removeDir(filePath.c_str())) {
        return false;
      }
    } else {
      if (!sd.remove(filePath.c_str())) {
        return false;
      }
    }
    file = dir.openNextFile();
  }

  return sd.rmdir(path);
}