#include "WifiSelectionActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <map>

#include "WifiCredentialStore.h"
#include "activities/util/ConfirmActivity.h"
#include "activities/util/KeyboardEntryActivity.h"

void WifiSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<WifiSelectionActivity*>(param);
  self->displayTaskLoop();
}

void WifiSelectionActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Load saved WiFi credentials - SD card operations need lock as we use SPI for both
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  WIFI_STORE.loadFromFile();
  xSemaphoreGive(renderingMutex);

  // Reset state
  selectedNetworkIndex = 0;
  networks.clear();
  state = WifiSelectionState::SCANNING;
  selectedSSID.clear();
  connectedIP.clear();
  connectionError.clear();
  enteredPassword.clear();
  usedSavedPassword = false;

  // Trigger first update to show scanning message
  updateRequired = true;

  xTaskCreate(&WifiSelectionActivity::taskTrampoline, "WifiSelectionTask",
              4096,               // Stack size (larger for WiFi operations)
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );

  // Start WiFi scan
  startWifiScan();
}

void WifiSelectionActivity::onExit() {
  Activity::onExit();

  Serial.printf("[%lu] [WIFI] [MEM] Free heap at onExit start: %d bytes\n", millis(), ESP.getFreeHeap());

  // Stop any ongoing WiFi scan
  Serial.printf("[%lu] [WIFI] Deleting WiFi scan...\n", millis());
  WiFi.scanDelete();
  Serial.printf("[%lu] [WIFI] [MEM] Free heap after scanDelete: %d bytes\n", millis(), ESP.getFreeHeap());

  // Note: We do NOT disconnect WiFi here - the parent activity (CrossPointWebServerActivity)
  // manages WiFi connection state. We just clean up the scan and task.

  // Acquire mutex before deleting task to ensure task isn't using it
  Serial.printf("[%lu] [WIFI] Acquiring rendering mutex before task deletion...\n", millis());
  xSemaphoreTake(renderingMutex, portMAX_DELAY);

  // Delete the display task
  Serial.printf("[%lu] [WIFI] Deleting display task...\n", millis());
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
    Serial.printf("[%lu] [WIFI] Display task deleted\n", millis());
  }

  // Now safe to delete the mutex since we own it
  Serial.printf("[%lu] [WIFI] Deleting mutex...\n", millis());
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
  Serial.printf("[%lu] [WIFI] Mutex deleted\n", millis());

  Serial.printf("[%lu] [WIFI] [MEM] Free heap at onExit end: %d bytes\n", millis(), ESP.getFreeHeap());
}

void WifiSelectionActivity::startWifiScan() {
  state = WifiSelectionState::SCANNING;
  networks.clear();
  updateRequired = true;

  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Start async scan
  WiFi.scanNetworks(true);  // true = async scan
}

void WifiSelectionActivity::processWifiScanResults() {
  const int16_t scanResult = WiFi.scanComplete();

  if (scanResult == WIFI_SCAN_RUNNING) {
    return;
  }

  if (scanResult == WIFI_SCAN_FAILED) {
    state = WifiSelectionState::NETWORK_LIST;
    updateRequired = true;
    return;
  }

  // Use a map to deduplicate networks by SSID, keeping the strongest signal
  std::map<std::string, WifiNetworkInfo> uniqueNetworks;

  for (int i = 0; i < scanResult; i++) {
    std::string ssid = WiFi.SSID(i).c_str();
    const int32_t rssi = WiFi.RSSI(i);

    if (ssid.empty()) continue;

    auto it = uniqueNetworks.find(ssid);
    if (it == uniqueNetworks.end() || rssi > it->second.rssi) {
      WifiNetworkInfo network;
      network.ssid = ssid;
      network.rssi = rssi;
      network.isEncrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      network.hasSavedPassword = (WIFI_STORE.getPassword(network.ssid) != nullptr);
      uniqueNetworks[ssid] = network;
    }
  }

  networks.clear();
  for (const auto& pair : uniqueNetworks) {
    // cppcheck-suppress useStlAlgorithm
    networks.push_back(pair.second);
  }

  // Sort by signal strength (strongest first)
  std::sort(networks.begin(), networks.end(),
            [](const WifiNetworkInfo& a, const WifiNetworkInfo& b) { return a.rssi > b.rssi; });

  // Show networks with saved password first
  std::sort(networks.begin(), networks.end(), [](const WifiNetworkInfo& a, const WifiNetworkInfo& b) {
    return a.hasSavedPassword && !b.hasSavedPassword;
  });

  WiFi.scanDelete();
  state = WifiSelectionState::NETWORK_LIST;
  selectedNetworkIndex = 0;
  updateRequired = true;
}

void WifiSelectionActivity::selectNetwork(const int index) {
  if (index < 0 || index >= static_cast<int>(networks.size())) return;

  const auto& network = networks[index];
  selectedSSID = network.ssid;
  selectedRequiresPassword = network.isEncrypted;
  usedSavedPassword = false;
  enteredPassword.clear();

  // Check if we have saved credentials for this network
  const auto* savedPassword = WIFI_STORE.getPassword(selectedSSID);
  if (savedPassword && !savedPassword->empty()) {
    enteredPassword = *savedPassword;
    usedSavedPassword = true;
    Serial.printf("[%lu] [WiFi] Using saved password for %s, length: %zu\n", millis(), selectedSSID.c_str(),
                  enteredPassword.size());
    attemptConnection();
    return;
  }

  if (selectedRequiresPassword) {
    state = WifiSelectionState::PASSWORD_ENTRY;
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    enterNewActivity(new KeyboardEntryActivity(
        renderer, inputManager, "Enter WiFi Password",
        "",
        50,
        64,
        false,
        [this](const std::string& text) {
          enteredPassword = text;
          exitActivity();
        },
        [this] {
          state = WifiSelectionState::NETWORK_LIST;
          updateRequired = true;
          exitActivity();
        }));
    updateRequired = true;
    xSemaphoreGive(renderingMutex);
  } else {
    attemptConnection();
  }
}

void WifiSelectionActivity::attemptConnection() {
  state = WifiSelectionState::CONNECTING;
  connectionStartTime = millis();
  connectedIP.clear();
  connectionError.clear();
  updateRequired = true;

  WiFi.mode(WIFI_STA);

  if (selectedRequiresPassword && !enteredPassword.empty()) {
    WiFi.begin(selectedSSID.c_str(), enteredPassword.c_str());
  } else {
    WiFi.begin(selectedSSID.c_str());
  }
}

void WifiSelectionActivity::checkConnectionStatus() {
  if (state != WifiSelectionState::CONNECTING) return;

  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    connectedIP = ipStr;

    if (!usedSavedPassword && !enteredPassword.empty()) {
      // Ask user if they want to save the new password
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      enterNewActivity(new ConfirmActivity(
          renderer, inputManager,
          "Save password for next time?",
          "Save Password",
          [this](bool confirmed) {
            if (confirmed) {
              xSemaphoreTake(renderingMutex, portMAX_DELAY);
              WIFI_STORE.addCredential(selectedSSID, enteredPassword);
              xSemaphoreGive(renderingMutex);
            }
            exitActivity();
            onComplete(true);
          }
      ));
      xSemaphoreGive(renderingMutex);
    } else {
      // Using saved password or open network - complete immediately
      Serial.printf("[%lu] [WIFI] Connected with saved/open credentials, completing immediately\n", millis());
      onComplete(true);
    }
    return;
  }

  if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
    connectionError = (status == WL_NO_SSID_AVAIL) ? "Network not found" : "Connection failed";
    state = WifiSelectionState::CONNECTION_FAILED;
    updateRequired = true;
    return;
  }

  if (millis() - connectionStartTime > CONNECTION_TIMEOUT_MS) {
    WiFi.disconnect();
    connectionError = "Connection timeout";
    state = WifiSelectionState::CONNECTION_FAILED;
    updateRequired = true;
  }
}

void WifiSelectionActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  if (state == WifiSelectionState::SCANNING) {
    processWifiScanResults();
    return;
  }

  if (state == WifiSelectionState::CONNECTING) {
    checkConnectionStatus();
    return;
  }

  if (state == WifiSelectionState::PASSWORD_ENTRY) {
    attemptConnection();
    return;
  }

  if (state == WifiSelectionState::CONNECTION_FAILED) {
    if (inputManager.wasPressed(InputManager::Button::Back) ||
        inputManager.wasPressed(InputManager::Button::Confirm)) {

      if (usedSavedPassword) {
        // Offer to forget the saved credentials that failed
        xSemaphoreTake(renderingMutex, portMAX_DELAY);
        enterNewActivity(new ConfirmActivity(
            renderer, inputManager,
            "Remove saved password?",
            "Forget Network?",
            [this](bool confirmed) {
              if (confirmed) {
                xSemaphoreTake(renderingMutex, portMAX_DELAY);
                WIFI_STORE.removeCredential(selectedSSID);
                xSemaphoreGive(renderingMutex);
                // Update the cached network list entry
                auto it = std::find_if(networks.begin(), networks.end(),
                                       [this](const WifiNetworkInfo& net) { return net.ssid == selectedSSID; });
                if (it != networks.end()) it->hasSavedPassword = false;
              }
              exitActivity();
              state = WifiSelectionState::NETWORK_LIST;
              updateRequired = true;
            }
        ));
        xSemaphoreGive(renderingMutex);
      } else {
        state = WifiSelectionState::NETWORK_LIST;
        updateRequired = true;
      }
    }
    return;
  }

  if (state == WifiSelectionState::NETWORK_LIST) {
    if (inputManager.wasPressed(InputManager::Button::Back)) {
      onComplete(false);
      return;
    }

    if (inputManager.wasPressed(InputManager::Button::Confirm)) {
      if (!networks.empty()) {
        selectNetwork(selectedNetworkIndex);
      } else {
        startWifiScan();
      }
      return;
    }

    if (inputManager.wasPressed(InputManager::Button::Up) ||
        inputManager.wasPressed(InputManager::Button::Left)) {
      if (!networks.empty()) {
        selectedNetworkIndex = (selectedNetworkIndex - 1 + networks.size()) % networks.size();
        updateRequired = true;
      }
    } else if (inputManager.wasPressed(InputManager::Button::Down) ||
               inputManager.wasPressed(InputManager::Button::Right)) {
      if (!networks.empty()) {
        selectedNetworkIndex = (selectedNetworkIndex + 1) % networks.size();
        updateRequired = true;
      }
    }
  }
}

std::string WifiSelectionActivity::getSignalStrengthIndicator(const int32_t rssi) const {
  if (rssi >= -50) return "||||";
  if (rssi >= -60) return "|||";
  if (rssi >= -70) return "||";
  if (rssi >= -80) return "|";
  return "";
}

void WifiSelectionActivity::displayTaskLoop() {
  while (true) {
    if (subActivity) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    if (state == WifiSelectionState::PASSWORD_ENTRY) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void WifiSelectionActivity::render() const {
  renderer.clearScreen();

  switch (state) {
    case WifiSelectionState::SCANNING:
      renderConnecting();
      break;
    case WifiSelectionState::NETWORK_LIST:
      renderNetworkList();
      break;
    case WifiSelectionState::CONNECTING:
      renderConnecting();
      break;
    case WifiSelectionState::CONNECTION_FAILED:
      renderConnectionFailed();
      break;
    default:
      break;
  }

  renderer.displayBuffer();
}

void WifiSelectionActivity::renderNetworkList() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.drawCenteredText(GfxRenderer::LARGE, 15, "WiFi Networks", true);

  if (networks.empty()) {
    const auto height = renderer.getLineHeight(GfxRenderer::MEDIUM);
    const auto top = (pageHeight - height) / 2;
    renderer.drawCenteredText(GfxRenderer::MEDIUM, top, "No networks found");
    renderer.drawCenteredText(GfxRenderer::SMALL, top + height + 10, "Press OK to scan again");
  } else {
    constexpr int startY = 60;
    constexpr int lineHeight = 30;
    const int maxVisibleNetworks = (pageHeight - startY - 40) / lineHeight;

    int scrollOffset = 0;
    if (selectedNetworkIndex >= maxVisibleNetworks) {
      scrollOffset = selectedNetworkIndex - maxVisibleNetworks + 1;
    }

    int displayIndex = 0;
    for (size_t i = scrollOffset; i < networks.size() && displayIndex < maxVisibleNetworks; i++, displayIndex++) {
      const int networkY = startY + displayIndex * lineHeight;
      const auto& network = networks[i];
      const bool isSelected = static_cast<int>(i) == selectedNetworkIndex;

      if (isSelected) {
        renderer.fillRect(0, networkY - 2, pageWidth - 1, 30);
      }

      std::string displayName = network.ssid;
      if (displayName.length() > 16) {
        displayName.replace(13, displayName.length() - 13, "...");
      }
      renderer.drawText(GfxRenderer::MEDIUM, 20, networkY, displayName.c_str(), !isSelected);

      const int nameWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, displayName.c_str());
      int xOffset = 20 + nameWidth;

      if (network.isEncrypted) {
        renderer.drawText(GfxRenderer::MEDIUM, xOffset + 3, networkY, "*", !isSelected);
        xOffset += 3 + renderer.getTextWidth(GfxRenderer::MEDIUM, "*");
      }

      if (network.hasSavedPassword) {
        renderer.drawText(GfxRenderer::MEDIUM, xOffset + 10, networkY, "+", !isSelected);
      }

      std::string signalStr = getSignalStrengthIndicator(network.rssi);
      if (!signalStr.empty()) {
        const int signalWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, signalStr.c_str());
        renderer.drawText(GfxRenderer::MEDIUM, pageWidth - signalWidth - 20, networkY, signalStr.c_str(), !isSelected);
      }
    }

    if (scrollOffset > 0) {
      renderer.drawText(GfxRenderer::SMALL, pageWidth - 15, startY - 10, "^");
    }
    if (scrollOffset + maxVisibleNetworks < static_cast<int>(networks.size())) {
      renderer.drawText(GfxRenderer::SMALL, pageWidth - 15, startY + maxVisibleNetworks * lineHeight, "v");
    }

    char countStr[32];
    snprintf(countStr, sizeof(countStr), "%zu networks found", networks.size());
    renderer.drawText(GfxRenderer::SMALL, 20, pageHeight - 50, countStr);
  }

  renderer.drawText(GfxRenderer::SMALL, 20, pageHeight - 30, "* = Encrypted | + = Saved");
}

void WifiSelectionActivity::renderConnecting() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto height = renderer.getLineHeight(GfxRenderer::MEDIUM);
  const auto top = (pageHeight - height) / 2;

  if (state == WifiSelectionState::SCANNING) {
    renderer.drawCenteredText(GfxRenderer::MEDIUM, top, "Scanning...");
  } else {
    renderer.drawCenteredText(GfxRenderer::LARGE, top - 40, "Connecting...", true);

    std::string ssidInfo = "to " + selectedSSID;
    if (ssidInfo.length() > 25) {
      ssidInfo.replace(22, ssidInfo.length() - 22, "...");
    }
    renderer.drawCenteredText(GfxRenderer::MEDIUM, top, ssidInfo.c_str());
  }
}

void WifiSelectionActivity::renderConnectionFailed() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto height = renderer.getLineHeight(GfxRenderer::MEDIUM);
  const auto top = (pageHeight - height * 2) / 2;

  renderer.drawCenteredText(GfxRenderer::LARGE, top - 20, "Connection Failed", true);
  renderer.drawCenteredText(GfxRenderer::MEDIUM, top + 20, connectionError.c_str());
  renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight - 30, "Press any button to continue");
}