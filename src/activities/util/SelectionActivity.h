#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

/**
 * Base class for list selection activities with:
 * - Background rendering task for responsive input
 * - Keyboard navigation (up/down/left/right)
 * - Page skipping when holding buttons
 * - Automatic scrolling for long lists
 * - Customizable item rendering
 */
class SelectionActivity : public Activity {
 protected:
  // Task and synchronization
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  bool updateRequired = false;

  // Selection state
  int selectedIndex = 0;
  std::string title;

  // Configuration
  static constexpr int DEFAULT_START_Y = 60;
  static constexpr int DEFAULT_LINE_HEIGHT = 30;
  static constexpr unsigned long SKIP_PAGE_MS = 700;

  // Override these in derived classes
  virtual int getItemCount() const = 0;
  virtual void renderItem(int index, int x, int y, bool isSelected) const = 0;
  virtual void onItemSelected(int index) = 0;
  virtual void onBack() = 0;

  // Optional overrides
  virtual int getStartY() const { return DEFAULT_START_Y; }
  virtual int getLineHeight() const { return DEFAULT_LINE_HEIGHT; }
  virtual bool showBattery() const { return true; }
  virtual void renderCustomHeader() const {}
  virtual void renderCustomFooter() const {}

  // Helper methods
  int getPageItems() const;
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