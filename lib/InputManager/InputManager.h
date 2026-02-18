#pragma once
#include <Arduino.h>

class InputManager {
 public:
  // Physical button indices (match hardware)
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;

  // Semantic button names (use hardware indices directly)
  enum Button {
    Back = BTN_BACK,
    Confirm = BTN_CONFIRM,
    Left = BTN_LEFT,
    Right = BTN_RIGHT,
    Up = BTN_UP,
    Down = BTN_DOWN,
    Power = BTN_POWER,
    PageBack = BTN_UP,      // Alias
    PageForward = BTN_DOWN  // Alias
  };

  // Pins
  static constexpr int BUTTON_ADC_PIN_1 = 1;
  static constexpr int BUTTON_ADC_PIN_2 = 2;
  static constexpr int POWER_BUTTON_PIN = 3;

  InputManager();
  void begin();
  void update();

  bool isPressed(Button button) const { return currentState & (1 << button); }
  bool wasPressed(Button button) const { return pressedEvents & (1 << button); }
  bool wasReleased(Button button) const { return releasedEvents & (1 << button); }
  bool wasAnyPressed() const { return pressedEvents > 0; }

  unsigned long getHeldTime() const {
    return (currentState > 0) ? (millis() - buttonPressStart) : 0;
  }

 private:
  uint8_t getState();
  static int getButtonFromADC(int adcValue, const int ranges[], int numButtons);

  uint8_t currentState;
  uint8_t lastState;
  uint8_t pressedEvents;
  uint8_t releasedEvents;
  unsigned long lastDebounceTime;
  unsigned long buttonPressStart;

  static constexpr int NUM_BUTTONS_1 = 4;
  static const int ADC_RANGES_1[];

  static constexpr int NUM_BUTTONS_2 = 2;
  static const int ADC_RANGES_2[];

  static constexpr int ADC_NO_BUTTON = 3800;
  static constexpr unsigned long DEBOUNCE_DELAY = 5;
};