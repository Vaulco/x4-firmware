#pragma once
#include <functional>
#include <string>

#include "activities/util/SelectionActivity.h"

// Enum for network mode selection
enum class NetworkMode { JOIN_NETWORK, CREATE_HOTSPOT };

namespace {
constexpr int networkModeCount = 2;
const char* networkModeNames[networkModeCount] = {
    "Join a Network",
    "Create Hotspot"
};
}  // namespace

class NetworkModeSelectionActivity final : public SelectionActivity {
  const std::function<void(NetworkMode)> onModeSelected;
  const std::function<void()> onCancel;

 protected:
  int getItemCount() const override {
    return networkModeCount;
  }

  void renderItem(int index, int x, int y, bool isSelected) const override {
    if (index < 0 || index >= networkModeCount) {
      return;
    }
    renderer.drawText(GfxRenderer::MEDIUM, x, y, networkModeNames[index], !isSelected);
  }

  void onItemSelected(int index) override {
    if (index < 0 || index >= networkModeCount) {
      return;
    }
    const NetworkMode mode = (index == 0) ? NetworkMode::JOIN_NETWORK : NetworkMode::CREATE_HOTSPOT;
    onModeSelected(mode);
  }

  void onBack() override {
    onCancel();
  }

 public:
  explicit NetworkModeSelectionActivity(GfxRenderer& renderer, InputManager& inputManager,
                                        const std::function<void(NetworkMode)>& onModeSelected,
                                        const std::function<void()>& onCancel)
      : SelectionActivity("NetworkModeSelection", "File Transfer", renderer, inputManager),
        onModeSelected(onModeSelected),
        onCancel(onCancel) {}
};