#pragma once

#include <ESP8266WiFi.h>

inline void applyWifiPower(bool active = false) {
  // Keep recovery beacons, reconnection and uploads awake. Zero listen interval
  // follows every router DTIM rather than skipping broadcast delivery.
  const auto mode = !active && WiFi.getMode() == WIFI_STA &&
      WiFi.status() == WL_CONNECTED ? WIFI_MODEM_SLEEP : WIFI_NONE_SLEEP;
  if (WiFi.getSleepMode() != mode && !WiFi.setSleepMode(mode, 0)) {
    Serial.println(F("Wi-Fi power mode change failed"));
  }
}

class ActiveWifiTransfer {
 public:
  ActiveWifiTransfer() { applyWifiPower(true); }
  ~ActiveWifiTransfer() { applyWifiPower(); }
  ActiveWifiTransfer(const ActiveWifiTransfer &) = delete;
  ActiveWifiTransfer &operator=(const ActiveWifiTransfer &) = delete;
};
