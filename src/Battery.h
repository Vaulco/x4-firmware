#pragma once
#include <cstdint>
#include <esp32-hal-adc.h>
#include <esp_adc_cal.h>

#define BAT_GPIO0 0

inline float min(const float a, const float b) { return a < b ? a : b; }
inline float max(const float a, const float b) { return a > b ? a : b; }

class Battery {
public:
  explicit Battery(int pin, float dividerMultiplier = 2.0f) 
      : adcPin(pin), dividerMultiplier(dividerMultiplier) {}

  uint16_t readPercentage() const {
    return percentageFromMillivolts(readMillivolts());
  }

private:
  uint8_t adcPin;
  float dividerMultiplier;

  uint16_t readMillivolts() const {
    const uint16_t raw = analogRead(adcPin);
    const uint32_t mv = millivoltsFromRawAdc(raw);
    return static_cast<uint32_t>(mv * dividerMultiplier);
  }

  static uint16_t percentageFromMillivolts(uint16_t millivolts) {
    double volts = millivolts / 1000.0;
    double y = -144.9390 * volts * volts * volts +
               1655.8629 * volts * volts -
               6158.8520 * volts +
               7501.3202;
    y = max(y, 0.0);
    y = min(y, 100.0);
    return static_cast<int>(round(y));
  }

  static uint16_t millivoltsFromRawAdc(uint16_t adc_raw) {
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    return esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
  }
};

inline Battery battery(BAT_GPIO0);