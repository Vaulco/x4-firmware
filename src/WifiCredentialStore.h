#pragma once
#include <string>
#include <map>

class WifiCredentialStore {
private:
  static WifiCredentialStore instance;
  std::map<std::string, std::string> credentials; // SSID -> password
  
  WifiCredentialStore() = default;
  
public:
  WifiCredentialStore(const WifiCredentialStore&) = delete;
  WifiCredentialStore& operator=(const WifiCredentialStore&) = delete;
  
  static WifiCredentialStore& getInstance() { return instance; }
  
  bool saveToFile() const;
  bool loadFromFile();
  bool addCredential(const std::string& ssid, const std::string& password);
  bool removeCredential(const std::string& ssid);
  const std::string* getPassword(const std::string& ssid) const;
  void clearAll();
};

#define WIFI_STORE WifiCredentialStore::getInstance()