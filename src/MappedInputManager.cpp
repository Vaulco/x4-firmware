#include "MappedInputManager.h"

#include "CrossPointSettings.h"

decltype(InputManager::BTN_BACK) MappedInputManager::mapButton(const Button button) const {
  // Always use default layout: Back, Confirm, Left, Right
  switch (button) {
    case Button::Back:
      return InputManager::BTN_BACK;
    case Button::Confirm:
      return InputManager::BTN_CONFIRM;
    case Button::Left:
      return InputManager::BTN_LEFT;
    case Button::Right:
      return InputManager::BTN_RIGHT;
    case Button::Up:
      return InputManager::BTN_UP;
    case Button::Down:
      return InputManager::BTN_DOWN;
    case Button::Power:
      return InputManager::BTN_POWER;
    case Button::PageBack:
      return InputManager::BTN_UP;
    case Button::PageForward:
      return InputManager::BTN_DOWN;
  }

  return InputManager::BTN_BACK;
}

bool MappedInputManager::wasPressed(const Button button) const { return inputManager.wasPressed(mapButton(button)); }

bool MappedInputManager::wasReleased(const Button button) const { return inputManager.wasReleased(mapButton(button)); }

bool MappedInputManager::isPressed(const Button button) const { return inputManager.isPressed(mapButton(button)); }

bool MappedInputManager::wasAnyPressed() const { return inputManager.wasAnyPressed(); }

bool MappedInputManager::wasAnyReleased() const { return inputManager.wasAnyReleased(); }

unsigned long MappedInputManager::getHeldTime() const { return inputManager.getHeldTime(); }

MappedInputManager::Labels MappedInputManager::mapLabels(const char* back, const char* confirm, const char* previous,
                                                         const char* next) const {
  // Always use default layout: Back, Confirm, Left, Right
  return {back, confirm, previous, next};
}