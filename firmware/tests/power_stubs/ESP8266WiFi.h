#pragma once
#include <cstdint>
#define F(value) value
enum WiFiMode_t { WIFI_STA, WIFI_AP, WIFI_AP_STA };
enum WiFiSleepType_t { WIFI_NONE_SLEEP = 0, WIFI_MODEM_SLEEP = 2 };
constexpr int WL_CONNECTED = 3;
struct FakeWifi {
  WiFiMode_t mode = WIFI_STA;
  int connection = WL_CONNECTED;
  WiFiSleepType_t sleep = WIFI_NONE_SLEEP;
  unsigned changes = 0;
  bool accept = true;
  WiFiMode_t getMode() const { return mode; }
  int status() const { return connection; }
  WiFiSleepType_t getSleepMode() const { return sleep; }
  bool setSleepMode(WiFiSleepType_t next, uint8_t interval) {
    if (interval != 0) __builtin_trap();
    ++changes;
    if (accept) sleep = next;
    return accept;
  }
};
inline FakeWifi WiFi;
struct FakeSerial {
  unsigned errors = 0;
  void println(const char *) { ++errors; }
};
inline FakeSerial Serial;
