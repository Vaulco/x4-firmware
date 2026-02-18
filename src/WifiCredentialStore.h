#pragma once
#include <string>
#include <map>
#include <SDCardManager.h>
#include <Serialization.h>

class WifiCredentialStore {
private:
  static WifiCredentialStore instance;
  std::map<std::string, std::string> credentials; // SSID -> password

  WifiCredentialStore() = default;

  static constexpr char WIFI_FILE[] = "/.ereader/wifi.bin";

public:
  WifiCredentialStore(const WifiCredentialStore&) = delete;
  WifiCredentialStore& operator=(const WifiCredentialStore&) = delete;

  static WifiCredentialStore& getInstance() { return instance; }

  bool saveToFile() const {
    SdMan.mkdir("/.ereader");

    FsFile file;
    if (!SdMan.openFileForWrite("WCS", WIFI_FILE, file)) {
      return false;
    }

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

  bool loadFromFile() {
    FsFile file;
    if (!SdMan.openFileForRead("WCS", WIFI_FILE, file)) {
      return false;
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

  bool addCredential(const std::string& ssid, const std::string& password) {
    credentials[ssid] = password;
    Serial.printf("[%lu] [WCS] Saved credential for: %s\n", millis(), ssid.c_str());
    return saveToFile();
  }

  bool removeCredential(const std::string& ssid) {
    if (credentials.erase(ssid) > 0) {
      Serial.printf("[%lu] [WCS] Removed credential for: %s\n", millis(), ssid.c_str());
      return saveToFile();
    }
    return false;
  }

  const std::string* getPassword(const std::string& ssid) const {
    auto it = credentials.find(ssid);
    return (it != credentials.end()) ? &it->second : nullptr;
  }

  void clearAll() {
    credentials.clear();
    saveToFile();
    Serial.printf("[%lu] [WCS] Cleared all credentials\n", millis());
  }
};

inline WifiCredentialStore WifiCredentialStore::instance;

#define WIFI_STORE WifiCredentialStore::getInstance()