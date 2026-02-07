#pragma once

#include <Arduino.h>

class InputManager {
 public:
  // Semantic button names for easier reading
  enum Button {
    Back,
    Confirm,
    Left,
    Right,
    Up,
    Down,
    Power,
    PageBack,    // Alias for Up
    PageForward  // Alias for Down
  };

  // Label structure for displaying button instructions
  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  InputManager();
  void begin();

  /**
   * Updates the button states. Should be called regularly in the main loop.
   */
  void update();

  /**
   * Returns true if the button was being held at the time of the last #update() call.
   *
   * @param button the semantic button name
   * @return the button current press state
   */
  bool isPressed(Button button) const;

  /**
   * Returns true if the button went from unpressed to pressed between the last two #update() calls.
   *
   * This differs from #isPressed() in that pressing and holding a button will cause this function
   * to return true after the first #update() call, but false on subsequent calls, whereas #isPressed()
   * will continue to return true.
   *
   * @param button the semantic button name
   * @return the button pressed state
   */
  bool wasPressed(Button button) const;

  /**
   * Returns true if any button started being pressed between the last two #update() calls
   *
   * @return true if any button started being pressed between the last two #update() calls
   */
  bool wasAnyPressed() const { return pressedEvents > 0; }

  /**
   * Returns true if the button went from pressed to unpressed between the last two #update() calls
   *
   * @param button the semantic button name
   * @return the button release state
   */
  bool wasReleased(Button button) const;

  /**
   * Returns true if any button was released between the last two #update() calls
   *
   * @return  true if any button was released between the last two #update() calls
   */
  bool wasAnyReleased() const { return releasedEvents > 0; }

  /**
   * Returns the time between any button starting to be depressed and all buttons between released
   *
   * @return duration in milliseconds
   */
  unsigned long getHeldTime() const;

  /**
   * Create button labels for display
   * 
   * @param back Label for back button
   * @param confirm Label for confirm button
   * @param previous Label for previous button
   * @param next Label for next button
   * @return Labels structure with button labels
   */
  static Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next);

  // Physical button indices (internal use)
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;

  // Pins
  static constexpr int BUTTON_ADC_PIN_1 = 1;
  static constexpr int BUTTON_ADC_PIN_2 = 2;
  static constexpr int POWER_BUTTON_PIN = 3;

 private:
  uint8_t getState();
  int getButtonFromADC(int adcValue, const int ranges[], int numButtons);
  static uint8_t mapButton(Button button);

  uint8_t currentState;
  uint8_t lastState;
  uint8_t pressedEvents;
  uint8_t releasedEvents;
  unsigned long lastDebounceTime;
  unsigned long buttonPressStart;
  unsigned long buttonPressFinish;

  static constexpr int NUM_BUTTONS_1 = 4;
  static const int ADC_RANGES_1[];

  static constexpr int NUM_BUTTONS_2 = 2;
  static const int ADC_RANGES_2[];

  static constexpr int ADC_NO_BUTTON = 3800;
  static constexpr unsigned long DEBOUNCE_DELAY = 5;
};