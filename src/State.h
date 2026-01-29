#pragma once
#include <iosfwd>
#include <string>

class State {
  // Static instance
  static State instance;

 public:
  std::string openEpubPath;
  ~State() = default;

  // Get singleton instance
  static State& getInstance() { return instance; }

  bool saveToFile() const;

  bool loadFromFile();
};

// Helper macro to access settings
#define APP_STATE State::getInstance()
