#pragma once

#include <WString.h>
#include <vector>
#include <SdFat.h>

class SDCardManager {
 public:
  SDCardManager();
  bool begin();
  bool ready() const;
  
  // File/directory listing
  std::vector<String> listFiles(const char* path = "/", int maxFiles = 200);
  
  // Directory operations
  bool ensureDirectoryExists(const char* path);
  bool removeDir(const char* path);
  
  // Simple wrappers for common SdFat operations
  FsFile open(const char* path, oflag_t oflag = O_RDONLY) { return sd.open(path, oflag); }
  bool mkdir(const char* path, bool pFlag = true) { return sd.mkdir(path, pFlag); }
  bool exists(const char* path) { return sd.exists(path); }
  bool remove(const char* path) { return sd.remove(path); }
  bool rmdir(const char* path) { return sd.rmdir(path); }

  // Helper for opening files with error logging
  bool openFileForRead(const char* moduleName, const char* path, FsFile& file);
  bool openFileForRead(const char* moduleName, const std::string& path, FsFile& file);
  bool openFileForRead(const char* moduleName, const String& path, FsFile& file);
  bool openFileForWrite(const char* moduleName, const char* path, FsFile& file);
  bool openFileForWrite(const char* moduleName, const std::string& path, FsFile& file);
  bool openFileForWrite(const char* moduleName, const String& path, FsFile& file);

  static SDCardManager& getInstance() { return instance; }

 private:
  static SDCardManager instance;
  bool initialized = false;
  SdFat sd;
};

#define SdMan SDCardManager::getInstance()