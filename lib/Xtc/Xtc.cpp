#include "Xtc.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>

bool Xtc::load() {
  Serial.printf("[%lu] [XTC] Loading XTC: %s\n", millis(), filepath.c_str());

  // Initialize parser
  parser.reset(new xtc::XtcParser());

  // Open XTC file
  xtc::XtcError err = parser->open(filepath.c_str());
  if (err != xtc::XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to load: %s\n", millis(), xtc::errorToString(err));
    parser.reset();
    return false;
  }

  loaded = true;
  Serial.printf("[%lu] [XTC] Loaded XTC: %s (%lu pages)\n", millis(), filepath.c_str(), parser->getPageCount());
  return true;
}

bool Xtc::clearCache() const {
  if (!SdMan.exists(cachePath.c_str())) {
    Serial.printf("[%lu] [XTC] Cache does not exist, no action needed\n", millis());
    return true;
  }

  if (!SdMan.removeDir(cachePath.c_str())) {
    Serial.printf("[%lu] [XTC] Failed to clear cache\n", millis());
    return false;
  }

  Serial.printf("[%lu] [XTC] Cache cleared successfully\n", millis());
  return true;
}

void Xtc::setupCacheDir() const {
    std::string current;
    for (size_t i = 0; i < cachePath.length(); i++) {
        current += cachePath[i];
        if (cachePath[i] == '/' || i == cachePath.length() - 1) {
            if (!SdMan.exists(current.c_str())) {
                SdMan.mkdir(current.c_str());
            }
        }
    }
}

std::string Xtc::getTitle() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get title from XTC metadata first
  std::string title = parser->getTitle();
  if (!title.empty()) {
    return title;
  }

  // Fallback: extract filename from path as title
  size_t lastSlash = filepath.find_last_of('/');
  size_t lastDot = filepath.find_last_of('.');

  if (lastSlash == std::string::npos) {
    lastSlash = 0;
  } else {
    lastSlash++;
  }

  if (lastDot == std::string::npos || lastDot <= lastSlash) {
    return filepath.substr(lastSlash);
  }

  return filepath.substr(lastSlash, lastDot - lastSlash);
}

bool Xtc::hasChapters() const {
  if (!loaded || !parser) {
    return false;
  }
  return parser->hasChapters();
}

const std::vector<xtc::ChapterInfo>& Xtc::getChapters() const {
  static const std::vector<xtc::ChapterInfo> kEmpty;
  if (!loaded || !parser) {
    return kEmpty;
  }
  return parser->getChapters();
}

uint32_t Xtc::getPageCount() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getPageCount();
}

uint16_t Xtc::getPageWidth() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getWidth();
}

uint16_t Xtc::getPageHeight() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getHeight();
}

uint8_t Xtc::getBitDepth() const {
  if (!loaded || !parser) {
    return 1;  // Default to 1-bit
  }
  return parser->getBitDepth();
}

bool Xtc::getPageInfo(uint32_t pageIndex, xtc::PageInfo& info) const {
  if (!loaded || !parser) {
    return false;
  }
  return parser->getPageInfo(pageIndex, info);
}

size_t Xtc::loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->loadPage(pageIndex, buffer, bufferSize);
}

xtc::XtcError Xtc::getLastError() const {
  if (!parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return parser->getLastError();
}