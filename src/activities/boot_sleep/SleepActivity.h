#pragma once
#include "../Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sleep", renderer, mappedInput) {}
  void onEnter() override;

 private:
  bool hasCustomSleepImages() const;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap) const;
};