#pragma once

#include <SdFat.h>
#include <WebServer.h>

#include <vector>

// Structure to hold file information
struct FileInfo {
  String name;
  size_t size;
  bool isDirectory;
};

class CrossPointWebServer {
 public:
  CrossPointWebServer();
  ~CrossPointWebServer();

  // Start the web server (call after WiFi is connected)
  void begin();

  // Stop the web server
  void stop();

  // Call this periodically to handle client requests
  void handleClient() const;

  // Check if server is running
  bool isRunning() const { return running; }

  // Get the port number
  uint16_t getPort() const { return port; }

 private:
  std::unique_ptr<WebServer> server = nullptr;
  bool running = false;
  bool apMode = false;  // true when running in AP mode, false for STA mode
  uint16_t port = 80;

  // Upload state (member variables instead of static)
  FsFile uploadFile;
  String uploadFileName;
  String uploadPath = "/";
  size_t uploadSize = 0;
  bool uploadSuccess = false;
  String uploadError = "";

  // Helper methods
  String getIPAddress() const;
  static bool isHiddenOrProtected(const String& name);
  static String normalizePath(String path);

  // File scanning
  void scanFiles(const char* path, const std::function<void(FileInfo)>& callback) const;

  // Request handlers
  void handleNotFound() const;
  void handleStatus() const;
  void handleFileList() const;
  void handleFileListData() const;
  void handleUpload() const;
  void handleUploadPost() const;
  void handleCreateFolder() const;
  void handleDelete() const;
};