#include <cassert>
#include <initializer_list>
#include "WifiPower.h"

static void failedTransfer() {
  ActiveWifiTransfer transfer;
  assert(WiFi.sleep == WIFI_NONE_SLEEP);
  return; // DNS/HTTP/JSON errors must all unwind the transfer guard.
}

int main() {
  applyWifiPower();
  assert(WiFi.sleep == WIFI_MODEM_SLEEP);
  const auto changes = WiFi.changes;
  applyWifiPower();
  assert(WiFi.changes == changes);
  failedTransfer();
  assert(WiFi.sleep == WIFI_MODEM_SLEEP);
  {
    ActiveWifiTransfer transfer;
    WiFi.connection = 0;
  }
  assert(WiFi.sleep == WIFI_NONE_SLEEP);
  WiFi.connection = WL_CONNECTED;
  for (auto mode : {WIFI_AP, WIFI_AP_STA}) {
    WiFi.mode = mode;
    applyWifiPower();
    assert(WiFi.sleep == WIFI_NONE_SLEEP);
  }
  WiFi.mode = WIFI_STA;
  applyWifiPower();
  applyWifiPower(true); // OTA starts; stays awake until completion/abort.
  assert(WiFi.sleep == WIFI_NONE_SLEEP);
  applyWifiPower();
  assert(WiFi.sleep == WIFI_MODEM_SLEEP);
  WiFi.sleep = WIFI_NONE_SLEEP;
  WiFi.accept = false;
  applyWifiPower();
  assert(WiFi.sleep == WIFI_NONE_SLEEP && Serial.errors == 1);
  WiFi.accept = true;
  applyWifiPower();
  assert(WiFi.sleep == WIFI_MODEM_SLEEP);
}
