#pragma once
#include <BatteryMonitor.h>

#define BAT_GPIO0 0  // Battery voltage

class GfxRenderer;

class Battery {
 public:
  explicit Battery(int pin) : monitor(pin) {}

  uint16_t readPercentage() { return monitor.readPercentage(); }

  void draw(const GfxRenderer& renderer, int left, int top) const;

 private:
  BatteryMonitor monitor;
};

// Global instance
extern Battery battery;