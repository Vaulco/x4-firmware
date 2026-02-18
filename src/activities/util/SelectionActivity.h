#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <functional>
#include <string>

#include "../Activity.h"

/**
 * Base class for list selection activities with:
 * - Background rendering task for responsive input
 * - Keyboard navigation (up/down/left/right)
 * - Automatic scrolling for long lists
 * - Customizable item rendering
 */
class SelectionActivity : public Activity {
 protected:
  // Task and synchronization
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  std::atomic<bool> updateRequired{false};

  // Selection state
  int selectedIndex = 0;
  std::string title;

  // Configuration - can be overridden in constructor
  int startY = 60;
  int lineHeight = 30;
  bool showBatteryIndicator = true;

  // Pure virtual methods - MUST be implemented
  virtual int getItemCount() const = 0;
  virtual void renderItem(int index, int x, int y, bool isSelected) const = 0;
  virtual void onItemSelected(int index) = 0;
  virtual void onBack() = 0;

  // Helper to calculate visible items
  int getPageItems() const;

  // Main render function
  void render() const;

 private:
  static void taskTrampoline(void* param);
  void displayTaskLoop();

 public:
  explicit SelectionActivity(const std::string& name, const std::string& title,
                            GfxRenderer& renderer, InputManager& inputManager)
      : Activity(name, renderer, inputManager), title(title) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
};