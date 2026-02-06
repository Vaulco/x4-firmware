#include "WifiCredentialStore.h"
#include <SDCardManager.h>
#include <Serialization.h>

WifiCredentialStore WifiCredentialStore::instance;

namespace {
constexpr uint8_t WIFI_FILE_VERSION = 1;
constexpr char WIFI_FILE[] = "/.ereader/wifi.bin";
}

bool WifiCredentialStore::saveToFile() const {
  SdMan.mkdir("/.ereader");
  
  FsFile file;
  if (!SdMan.openFileForWrite("WCS", WIFI_FILE, file)) {
    return false;
  }
  
  serialization::writePod(file, WIFI_FILE_VERSION);
  serialization::writePod(file, static_cast<uint8_t>(credentials.size()));
  
  for (const auto& [ssid, password] : credentials) {
    serialization::writeString(file, ssid);

    // NOTE: Credentials are stored in plaintext.
    // Anyone with SD access can read WiFi passwords.
    serialization::writeString(file, password);
  }
  
  file.close();
  Serial.printf("[%lu] [WCS] Saved %zu WiFi credentials\n", millis(), credentials.size());
  return true;
}

bool WifiCredentialStore::loadFromFile() {
  FsFile file;
  if (!SdMan.openFileForRead("WCS", WIFI_FILE, file)) {
    return false;
  }
  
  uint8_t version;
  serialization::readPod(file, version);
  if (version != WIFI_FILE_VERSION) {
    Serial.printf("[%lu] [WCS] Unknown file version %u, clearing\n", millis(), version);
    file.close();
    credentials.clear();
    return saveToFile(); // Start fresh
  }
  
  uint8_t count;
  serialization::readPod(file, count);
  
  credentials.clear();
  for (uint8_t i = 0; i < count; i++) {
    std::string ssid, password;
    serialization::readString(file, ssid);
    serialization::readString(file, password);
    credentials[ssid] = password;
  }
  
  file.close();
  Serial.printf("[%lu] [WCS] Loaded %zu WiFi credentials\n", millis(), credentials.size());
  return true;
}

bool WifiCredentialStore::addCredential(const std::string& ssid, const std::string& password) {
  credentials[ssid] = password; // Overwrites if exists
  Serial.printf("[%lu] [WCS] Saved credential for: %s\n", millis(), ssid.c_str());
  return saveToFile();
}

bool WifiCredentialStore::removeCredential(const std::string& ssid) {
  if (credentials.erase(ssid) > 0) {
    Serial.printf("[%lu] [WCS] Removed credential for: %s\n", millis(), ssid.c_str());
    return saveToFile();
  }
  return false;
}

const std::string* WifiCredentialStore::getPassword(const std::string& ssid) const {
  auto it = credentials.find(ssid);
  return (it != credentials.end()) ? &it->second : nullptr;
}

void WifiCredentialStore::clearAll() {
  credentials.clear();
  saveToFile();
  Serial.printf("[%lu] [WCS] Cleared all credentials\n", millis());
}