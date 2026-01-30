#pragma once
#include <cstdint>

#define BAT_GPIO0 0  // Battery voltage

class GfxRenderer;

class Battery {
 public:
  explicit Battery(int pin, float dividerMultiplier = 2.0f) 
      : adcPin(pin), dividerMultiplier(dividerMultiplier) {}

  void draw(const GfxRenderer& renderer, int left, int top) const;

  // Read voltage and return percentage (0-100)
  uint16_t readPercentage() const;

  // Read the battery voltage in millivolts (accounts for divider)
  uint16_t readMillivolts() const;

  // Read raw millivolts from ADC (doesn't account for divider)
  uint16_t readRawMillivolts() const;

  // Read the battery voltage in volts (accounts for divider)
  double readVolts() const;

  // Percentage (0-100) from a millivolt value
  static uint16_t percentageFromMillivolts(uint16_t millivolts);

  // Calibrate a raw ADC reading and return millivolts
  static uint16_t millivoltsFromRawAdc(uint16_t adc_raw);

 private:
  uint8_t adcPin;
  float dividerMultiplier;
};

// Global instance
extern Battery battery;