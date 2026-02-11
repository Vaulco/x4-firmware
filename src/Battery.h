#pragma once
#include <cstdint>

#define BAT_GPIO0 0  // Battery voltage

class GfxRenderer;

class Battery {
 public:
  explicit Battery(int pin, float dividerMultiplier = 2.0f) 
      : adcPin(pin), dividerMultiplier(dividerMultiplier) {}

  // Read voltage and return percentage (0-100)
  uint16_t readPercentage() const;

 private:
  uint8_t adcPin;
  float dividerMultiplier;

  // Read the battery voltage in millivolts (accounts for divider)
  uint16_t readMillivolts() const;

  // Percentage (0-100) from a millivolt value
  static uint16_t percentageFromMillivolts(uint16_t millivolts);

  // Calibrate a raw ADC reading and return millivolts
  static uint16_t millivoltsFromRawAdc(uint16_t adc_raw);
};

// Global instance
extern Battery battery;