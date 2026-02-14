#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <esp32-hal-adc.h>
#include <esp_adc_cal.h>

#define BAT_GPIO0 0

class Battery {
public:
  explicit Battery(int pin, float dividerMultiplier = 2.0f) 
      : adcPin(pin), dividerMultiplier(dividerMultiplier) {
    // Calibrate ADC once during construction
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adcChars);
  }

  uint16_t readPercentage() const {
    return percentageFromMillivolts(readMillivolts());
  }

private:
  uint8_t adcPin;
  float dividerMultiplier;
  esp_adc_cal_characteristics_t adcChars;

  uint32_t readMillivolts() const {  // Changed to uint32_t
    const uint16_t raw = analogRead(adcPin);
    const uint32_t mv = millivoltsFromRawAdc(raw);
    return static_cast<uint32_t>(mv * dividerMultiplier);  // Safe now
  }

  // Polynomial fit for lithium battery voltage (3.0V-4.2V range)
  // Converts battery voltage to percentage (0-100%)
  static uint16_t percentageFromMillivolts(uint32_t millivolts) {  // Changed parameter
    double volts = millivolts / 1000.0;
    double y = -144.9390 * volts * volts * volts +
               1655.8629 * volts * volts -
               6158.8520 * volts +
               7501.3202;
    y = std::max(y, 0.0);
    y = std::min(y, 100.0);
    return static_cast<uint16_t>(std::round(y));
  }

  uint32_t millivoltsFromRawAdc(uint16_t adc_raw) const {  // Changed return type
    return esp_adc_cal_raw_to_voltage(adc_raw, &adcChars);
  }
};