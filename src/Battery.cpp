#include "Battery.h"

#include <GfxRenderer.h>
#include <esp32-hal-adc.h>
#include <esp_adc_cal.h>

#include <string>

#include "fontIds.h"

inline float min(const float a, const float b) { return a < b ? a : b; }
inline float max(const float a, const float b) { return a > b ? a : b; }

uint16_t Battery::readPercentage() const {
  return percentageFromMillivolts(readMillivolts());
}

uint16_t Battery::readMillivolts() const {
  const uint16_t raw = readRawMillivolts();
  const uint32_t mv = millivoltsFromRawAdc(raw);
  return static_cast<uint32_t>(mv * dividerMultiplier);
}

uint16_t Battery::readRawMillivolts() const {
  const uint16_t raw = analogRead(adcPin);
  return raw;
}

double Battery::readVolts() const {
  return static_cast<double>(readMillivolts()) / 1000.0;
}

uint16_t Battery::percentageFromMillivolts(uint16_t millivolts) {
  double volts = millivolts / 1000.0;
  // Polynomial derived from LiPo samples
  double y = -144.9390 * volts * volts * volts +
             1655.8629 * volts * volts -
             6158.8520 * volts +
             7501.3202;

  // Clamp to [0,100] and round
  y = max(y, 0.0);
  y = min(y, 100.0);
  y = round(y);
  return static_cast<int>(y);
}

uint16_t Battery::millivoltsFromRawAdc(uint16_t adc_raw) {
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
}

void Battery::draw(const GfxRenderer& renderer, int left, int top) const {
  const uint16_t percentage = readPercentage();

  // Draw percentage text
  const std::string text = std::to_string(percentage) + "%";
  renderer.drawText("cmu_8", left + 27, top, text.c_str());

  constexpr int bodyW = 20;
  constexpr int bodyH = 12;
  constexpr int tipW  = 2;

  const int x = left;
  const int y = top + 6;

  // Battery body (outline)
  renderer.drawRect(x, y, bodyW, bodyH);

  // Battery tip
  renderer.fillRect(x + bodyW, y + 3, tipW, bodyH - 6);

  // Fill level
  const int maxFill = bodyW - 4;
  int fillW = (percentage * maxFill + 99) / 100; // rounded
  renderer.fillRect(x + 2, y + 2, fillW, bodyH - 4);
}

// Define global instance
Battery battery(BAT_GPIO0);