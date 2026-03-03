#pragma once
#include <memory>
#include "Activity.h"

class ActivityWithSubactivity : public Activity {
 protected:
  std::unique_ptr<Activity> subActivity = nullptr;

  void exitActivity() {
    if (subActivity) {
      subActivity->onExit();
      subActivity.reset();
    }
  }

  void enterNewActivity(Activity* activity) {
    subActivity.reset(activity);
    subActivity->onEnter();
  }

 public:
  explicit ActivityWithSubactivity(std::string name, GfxRenderer& renderer, InputManager& inputManager)
      : Activity(std::move(name), renderer, inputManager) {}

  void loop() override {
    if (subActivity) subActivity->loop();
  }

  void onExit() override {
    Activity::onExit();
    exitActivity();
  }
};