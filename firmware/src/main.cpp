#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <Updater.h>
#include <TFT_eSPI.h>
#include <cmath>
#include "DeviceSettings.h"
#include "StoredConfig.h"
#include "StoredConfigFile.h"
#include "SeoulWeather.h"
#include "SeoulAirQuality.h"
#include "PageCycle.h"
#include "ForecastLabels.h"
#include "DisplaySettings.h"
#include "ApiRefresh.h"
#include "WifiRetry.h"
#include "fonts/InterTightBold18.h"
#include "fonts/InterTightBold24.h"
#include "fonts/InterTightBold36.h"
#include "fonts/InterTightDigits48.h"
#include "fonts/InterTightCompact13.h"
#include "WebAssets.generated.h"
#ifdef SDPRO_TICKER
#include "Ticker.h"
#endif

namespace {
#ifdef SDPRO_TICKER
constexpr char kVersion[] = "SDPRO-Ticker-1.0.0";
#else
constexpr char kVersion[] = "SeoulWeather-4.0.0";
#endif
constexpr uint32_t kServiceIntervalMs = 100;
constexpr uint32_t kBootIpDisplayMs = 5000;
constexpr uint16_t kDividerColor = 0x638F;  // Muted blue-grey on black.
DeviceConfig config{};
NetworkSettings networkSettings{};
CitySettings citySettings{};
DisplaySettings displaySettings{};
ESP8266WebServer server(80);
DNSServer dns;
TFT_eSPI display;
SeoulWeatherDay seoulWeather[3]{};
bool seoulWeatherValid = false;
SeoulAirQuality seoulAirQuality{};
bool airQualityValid = false;
bool filesystemReady = false;
bool accessPointRunning = false;
bool timeConfigured = false;
bool updating = false;
bool uploadStarted = false;
bool uploadSucceeded = false;
uint8_t wifiAttemptCount = 0;
uint8_t displayBrightness = 255;
time_t lastBacklightMinute = -1;
ApiRefresh weatherRefresh, airQualityRefresh;
bool weatherDirty = true, airQualityDirty = true;
time_t drawnPageMinute = 0;
uint32_t lastServiceAt = 0;
uint32_t connectStartedAt = 0;
uint32_t lastDrawAt = 0;
uint32_t restartAt = 0;
PageCycle pageCycle;
bool pagesStarted = false;
bool bootIpShown = false;
uint32_t bootIpShownAt = 0;
int lastWifiStatus = -1;
uint8_t lastWifiDisconnectReason = 0;
WiFiEventHandler wifiDisconnectHandler;

const char *wifiStatusText(int status) {
  if (!wifiConfigured(config)) return "Wi-Fi not configured";
  switch (status) {
    case WL_CONNECTED: return "Wi-Fi connected";
    case WL_NO_SSID_AVAIL: return "Wi-Fi network not found";
    case WL_WRONG_PASSWORD: return "Wi-Fi password rejected";
    case WL_CONNECT_FAILED: return "Wi-Fi connection failed";
    default: return "Waiting for Wi-Fi / IP";
  }
}

void drawRecoveryWifiStatus(int status) {
  display.fillRect(0, 155, 240, 42, TFT_BLACK);
  display.setFreeFont(&InterTightBold18);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(wifiStatusText(status), 120, 175);
}

void applyBacklight(bool force = false) {
  const time_t now = time(nullptr);
  const time_t minute = now / 60;
  if (!force && minute == lastBacklightMinute) return;
  lastBacklightMinute = minute;
  const bool valid = now > 1000000000;
  tm localNow{};
  if (valid) localtime_r(&now, &localNow);
  const uint8_t brightness = effectiveDisplayBrightness(displaySettings,
      localNow.tm_hour * 60 + localNow.tm_min, valid);
  if (brightness == displayBrightness) return;
  displayBrightness = brightness;
  // Verified SD PRO GPIO5 PWM control; its backlight is active LOW.
  pinMode(TFT_BL, OUTPUT);
  analogWriteRange(100);
  analogWriteFreq(1000);
  analogWrite(TFT_BL, TFT_BACKLIGHT_ON == LOW ? 100 - brightness : brightness);
}

void message(const String &title, const String &detail) {
  display.fillScreen(TFT_BLACK);
  display.setFreeFont(&InterTightBold18);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(title, 120, 80);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(detail, 120, 120);
  lastDrawAt = millis();
}

bool localRequest() {
  const uint32_t remote = server.client().remoteIP();
  const uint32_t apMask = IPAddress(255, 255, 255, 0);
  if (accessPointRunning &&
      (remote & apMask) == (uint32_t(WiFi.softAPIP()) & apMask)) return true;
  return WiFi.status() == WL_CONNECTED &&
      (remote & uint32_t(WiFi.subnetMask())) ==
      (uint32_t(WiFi.localIP()) & uint32_t(WiFi.subnetMask()));
}

bool authenticated(bool ota = false) {
  if (!localRequest()) {
    server.send(403, "text/plain", "Local network access required");
    return false;
  }
  if (ota && deviceConfigValid(config) && !config.directOtaEnabled) {
    server.send(403, "text/plain", "Direct OTA is disabled");
    return false;
  }
  if (!ota && accessPointRunning &&
      server.client().localIP() == WiFi.softAPIP()) return true;
  const bool required = ota ? config.otaAuthEnabled : config.apiAuthEnabled;
  if (!required) return true;
  const char *password = ota ? config.otaPassword : config.apiPassword;
  if (password[0] && server.authenticate(configuredUsername(config), password)) return true;
  server.requestAuthentication();
  return false;
}

void startAccessPoint() {
  if (accessPointRunning) return;
  WiFi.mode(wifiConfigured(config) ? WIFI_AP_STA : WIFI_AP);
  const String ssid = "SDPRO-Setup-" + deviceSuffix();
  const bool started = networkSettings.recoveryPassword[0]
      ? WiFi.softAP(ssid.c_str(), networkSettings.recoveryPassword)
      : WiFi.softAP(ssid.c_str());
  if (!started) { Serial.println(F("Recovery AP failed")); return; }
  accessPointRunning = true;
  dns.start(53, "*", WiFi.softAPIP());
  message(ssid, WiFi.softAPIP().toString());
  drawRecoveryWifiStatus(WiFi.status());
  Serial.printf("Setup portal: http://%s/\n", WiFi.softAPIP().toString().c_str());
}

void connectToWiFi() {
  WiFi.persistent(false);
  if (!wifiConfigured(config)) { startAccessPoint(); return; }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Keep web/OTA latency predictable until sleep is validated on this device.
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  configureIpAddress(networkSettings);
  WiFi.hostname(configuredHostname(config).c_str());
  WiFi.begin(config.ssid, config.wifiPassword);
  wifiAttemptCount = 1;
  connectStartedAt = millis();
  message("Connecting Wi-Fi", config.ssid);
}

void sendPage() {
  if (!authenticated()) return;
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8",
                reinterpret_cast<const char *>(kWebAppGzip), kWebAppGzipSize);
}

void sendStatus() {
  if (!authenticated()) return;
  StaticJsonDocument<1792> doc;
  doc["city"] = citySettings.city;
  doc["cityName"] = citySettings.name;
  doc["latitude"] = citySettings.latitude;
  doc["longitude"] = citySettings.longitude;
  doc["version"] = kVersion;
  doc["ssid"] = config.ssid;
  doc["wifiConfigured"] = wifiConfigured(config);
  doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
  doc["wifiStatus"] = static_cast<int>(WiFi.status());
  doc["wifiStatusText"] = wifiStatusText(WiFi.status());
  doc["lastWifiDisconnectReason"] = lastWifiDisconnectReason;
  doc["staticIpEnabled"] = networkSettings.staticIpEnabled;
  doc["hostname"] = configuredHostname(config);
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["dns"] = WiFi.dnsIP().toString();
  doc["freeHeapBytes"] = ESP.getFreeHeap();
  doc["maximumFreeBlockBytes"] = ESP.getMaxFreeBlockSize();
  doc["heapFragmentationPercent"] = ESP.getHeapFragmentation();
  doc["weatherValid"] = seoulWeatherValid;
  doc["weatherError"] = seoulWeatherLastErrorText;
  doc["weatherHttpCode"] = seoulWeatherLastHttpCode;
  doc["brightness"] = displayBrightness;
  encodeDisplaySettings(doc.createNestedObject("displaySettings"), displaySettings);
  doc["cpuMHz"] = ESP.getCpuFreqMHz();
  doc["wifiMode"] = static_cast<int>(WiFi.getMode());
  doc["wifiSleep"] = static_cast<int>(WiFi.getSleepMode());
  doc["airQualityValid"] = airQualityValid;
  doc["airQualityError"] = airQualityLastError;
  doc["airQualityHttpCode"] = airQualityLastHttpCode;
  doc["ntpServer"] = currentNtpServer(networkSettings);
  doc["timezone"] = kDefaultTimezone;
  doc["directOtaEnabled"] = config.directOtaEnabled != 0;
  doc["otaAuthEnabled"] = config.otaAuthEnabled != 0;
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

void saveSettings() {
  if (!authenticated()) return;
  const String ssid = server.arg("ssid").length() ? server.arg("ssid") : String(config.ssid);
  const String password = server.arg("password");
  const String host = server.arg("hostname").length() ? server.arg("hostname") : configuredHostname(config);
  const String user = server.arg("username").length() ? server.arg("username") : String(configuredUsername(config));
  const String admin = server.arg("adminPassword");
  const bool enableAuth = server.arg("authentication") == "on";
  // An empty admin password preserves existing credentials.
  if (!ssid.length() || ssid.length() >= sizeof(config.ssid) || password.length() > 64) {
    server.send(422, "text/plain", "Invalid Wi-Fi settings (SSID: 1-32 bytes, password: up to 64 characters)");
    return;
  }
  if (!hostnameValid(host.c_str()) || !usernameValid(user.c_str())) {
    server.send(422, "text/plain", "Invalid hostname or admin username");
    return;
  }
  if (admin.length() > 32 || (admin.length() && admin.length() < 8) ||
      (enableAuth && !admin.length() &&
          (strlen(config.apiPassword) < 8 || strlen(config.otaPassword) < 8))) {
    server.send(422, "text/plain", "Admin password required: enter 8-32 characters or uncheck admin authentication");
    return;
  }
  DeviceConfig next = config;
  strlcpy(next.ssid, ssid.c_str(), sizeof(next.ssid));
  if (password.length() || server.arg("openWifi") == "on")
    strlcpy(next.wifiPassword, password.c_str(), sizeof(next.wifiPassword));
  strlcpy(next.hostname, host.c_str(), sizeof(next.hostname));
  strlcpy(next.username, user.c_str(), sizeof(next.username));
  if (admin.length()) {
    strlcpy(next.apiPassword, admin.c_str(), sizeof(next.apiPassword));
    strlcpy(next.otaPassword, admin.c_str(), sizeof(next.otaPassword));
  }
  next.apiAuthEnabled = enableAuth;
  next.otaAuthEnabled = enableAuth;
  next.directOtaEnabled = 1;
  next.wifiRetryLimit = kDefaultWifiRetryLimit;
  NetworkSettings network = networkSettings;
  StaticJsonDocument<384> extras;
  if (server.arg("ntpServer").length()) {
    extras["ntpServer"] = server.arg("ntpServer");
    extras["ntpFromDhcp"] = false;
  }
  if (server.arg("dhcp") == "on") extras["staticIpEnabled"] = false;
  if (server.arg("recoveryPassword").length()) {
    extras["recoveryPasswordEnabled"] = true;
    extras["recoveryPassword"] = server.arg("recoveryPassword");
  }
  if (!networkExtrasValid(extras, network)) {
    server.send(422, "text/plain", "Invalid NTP or recovery AP password");
    return;
  }
  updateNetworkExtras(extras, network);
  if (!saveNetworkSettings(network, filesystemReady)) {
    server.send(500, "text/plain", "Could not save network settings"); return;
  }
  if (!saveDeviceConfig(next)) {
    server.send(500, "text/plain", "Could not save Wi-Fi settings. Device has not restarted; please retry.");
    return;
  }
  config = next;
  server.send(200, "text/plain", "Settings saved. Restarting...");
  restartAt = millis() + 500;
}

void drawCurrentPage();

void setCity() {
  if (!authenticated()) return;
  const String selection = server.arg("city");
  CitySettings next;
  if (selection == "CUSTOM") {
    String name = server.arg("cityName");
    name.trim();
    if (!cityNameValid(name.c_str()) ||
        !normalizeCoordinate(server.arg("latitude").c_str(), 90, next.latitude) ||
        !normalizeCoordinate(server.arg("longitude").c_str(), 180, next.longitude)) {
      server.send(422, "text/plain", "CUSTOM: name must be 1-20 printable ASCII characters; latitude -90 to 90, longitude -180 to 180, at most 6 decimals");
      return;
    }
    strcpy(next.city, "CUSTOM");
    strcpy(next.name, name.c_str());
  } else if (!selectPresetCity(selection.c_str(), next)) {
    server.send(422, "text/plain", "Invalid city");
    return;
  }
  if (!saveCitySettings(next, filesystemReady)) {
    server.send(500, "text/plain", "Could not save city settings");
    return;
  }
  citySettings = next;
  seoulWeatherValid = airQualityValid = false;
  for (auto &day : seoulWeather) day = SeoulWeatherDay{};
  seoulAirQuality = SeoulAirQuality{};
  weatherRefresh = airQualityRefresh = ApiRefresh{};
  seoulWeatherLastError = SeoulWeatherError::None;
  seoulWeatherLastHttpCode = airQualityLastHttpCode = 0;
  seoulWeatherLastErrorText = "Waiting for new city data";
  airQualityLastError = "Waiting for new city data";
  weatherDirty = airQualityDirty = true;
  // Clear visible old-city data before the next synchronous network request.
  if (pagesStarted) drawCurrentPage();
  server.sendHeader("Location", "/");
  server.send(303);
}

bool parseDisplayNumber(const String &text, uint16_t maximum, uint16_t &value) {
  if (!text.length() || text.length() > 3) return false;
  value = 0;
  for (unsigned i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = value * 10 + text[i] - '0';
  }
  return value <= maximum;
}

void setDisplaySettings() {
  if (!authenticated()) return;
  DisplaySettings next = displaySettings;
  const String rotate = server.arg("autoRotate"), night = server.arg("nightModeEnabled");
  const String fixed = server.arg("fixedPage");
  uint16_t weather, air, brightness, nightBrightness;
  if ((rotate != "true" && rotate != "false") || (night != "true" && night != "false") ||
      (!fixed.length() && rotate != "true") ||
      (fixed.length() && fixed != "WEATHER" && fixed != "AIR QUALITY") ||
      !parseDisplayNumber(server.arg("weatherPageSeconds"), 60, weather) || !pageSecondsValid(weather) ||
      !parseDisplayNumber(server.arg("airPageSeconds"), 60, air) || !pageSecondsValid(air) ||
      !parseDisplayNumber(server.arg("brightness"), 100, brightness) ||
      !((brightness >= 20 && brightness % 20 == 0) || brightness == displaySettings.brightness) ||
      !parseDisplayNumber(server.arg("nightBrightness"), 50, nightBrightness) || !nightBrightnessValid(nightBrightness) ||
      !parseDisplayTime(server.arg("nightStart").c_str(), next.nightStart) ||
      !parseDisplayTime(server.arg("nightEnd").c_str(), next.nightEnd)) {
    server.send(422, "text/plain", "Invalid display settings: choose listed values and HH:MM times");
    return;
  }
  next.autoRotate = rotate == "true";
  next.nightModeEnabled = night == "true";
  next.weatherPageSeconds = weather;
  next.airPageSeconds = air;
  next.brightness = brightness;
  next.nightBrightness = nightBrightness;
  if (fixed.length()) next.fixedPage = fixed == "WEATHER" ? ForecastPage::Weather : ForecastPage::AirQuality;
  if (!saveDisplaySettings(next, filesystemReady)) {
    server.send(500, "text/plain", "Could not save display settings");
    return;
  }
  displaySettings = next;
  pageCycle.configure(millis(), next.autoRotate, next.weatherPageSeconds,
                                          next.airPageSeconds, next.fixedPage);
  applyBacklight(true);
  if (pagesStarted) drawCurrentPage();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setBrightness() {
  if (!authenticated()) return;
  const String input = server.arg("brightness");
  if (!input.length() || input.length() > 3) {
    server.send(422, "text/plain", "Brightness must be 0-100"); return;
  }
  for (unsigned i = 0; i < input.length(); ++i) {
    if (input[i] < '0' || input[i] > '9') {
      server.send(422, "text/plain", "Brightness must be 0-100"); return;
    }
  }
  const int value = input.toInt();
  if (value > 100) { server.send(422, "text/plain", "Brightness must be 0-100"); return; }
  DisplaySettings next = displaySettings;
  next.brightness = value;
  if (!saveDisplaySettings(next, filesystemReady)) { server.send(500, "text/plain", "Could not save brightness"); return; }
  displaySettings = next;
  applyBacklight(true);
  server.sendHeader("Location", "/");
  server.send(303);
}

void resumeAfterUpdateFailure() {
  updating = false;
  timeConfigured = false;
  lastDrawAt = 0;
  weatherDirty = airQualityDirty = true;
  // OTA stops every UDP socket, including captive DNS and NTP.
  if (accessPointRunning) dns.start(53, "*", WiFi.softAPIP());
}

void receiveUpdate() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadStarted = uploadSucceeded = false;
    if (!authenticated(true)) return;
    updating = true;
    message("Firmware update", "Do not power off");
    WiFiUDP::stopAll();
    uploadStarted = Update.begin(ESP.getFreeSketchSpace() & 0xFFFFF000);
    if (!uploadStarted) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE && uploadStarted) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END && uploadStarted) {
    uploadSucceeded = Update.end(true);
    if (!uploadSucceeded) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadStarted) Update.end();
    uploadStarted = uploadSucceeded = false;
    resumeAfterUpdateFailure();
  }
  yield();
}

void finishUpdate() {
  if (!authenticated(true)) return;
  const bool success = uploadStarted && uploadSucceeded && !Update.hasError();
  server.send(success ? 200 : 500, "text/plain",
      success ? "Update complete. Restarting..." : "Update failed");
  uploadStarted = uploadSucceeded = updating = false;
  if (success) restartAt = millis() + 500;
  else resumeAfterUpdateFailure();
}

void configureRoutes() {
#ifdef SDPRO_TICKER
  tickerRoutes(server, [] { return authenticated(); });
#else
  server.on("/", HTTP_GET, sendPage);
#endif
  server.on("/network", HTTP_GET, sendPage);
  server.on("/update", HTTP_GET, sendPage);
  server.on("/settings", HTTP_POST, saveSettings);
  server.on("/brightness", HTTP_POST, setBrightness);
  server.on("/display-settings", HTTP_POST, setDisplaySettings);
  server.on("/city", HTTP_POST, setCity);
  server.on("/update", HTTP_POST, finishUpdate, receiveUpdate);
  server.on("/update_ota", HTTP_POST, finishUpdate, receiveUpdate);
  server.on("/api/v1/firmware", HTTP_POST, finishUpdate, receiveUpdate);
  server.on("/api/v1/status", HTTP_GET, sendStatus);
  server.on("/api/v1/info", HTTP_GET, sendStatus);
  server.onNotFound([] {
    if (accessPointRunning && server.method() == HTTP_GET) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "Open setup portal");
    } else server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

#ifndef SDPRO_TICKER
// Scale primitives for today and compact forecasts without a sprite or bitmap.
void drawWeatherIcon(int16_t x, int16_t y, int weatherCode, int16_t size) {
  const auto scale = [size](int16_t v) -> int16_t { return (v * size + 14) / 28; };
  const int16_t stroke = size >= 60 ? 3 : size >= 36 ? 2 : 1;
  display.fillRect(x, y, size, size, TFT_BLACK);
  const auto circle = [&](int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    display.fillCircle(x+scale(cx), y+scale(cy), scale(r), color);
  };
  const auto outlineCircle = [&](int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    for (int16_t inset = 0; inset < stroke; ++inset)
      display.drawCircle(x+scale(cx), y+scale(cy), scale(r)-inset, color);
  };
  const auto line = [&](int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    for (int16_t offset = -(stroke/2); offset <= stroke/2; ++offset)
      display.drawLine(x+scale(x1)+offset, y+scale(y1),
                       x+scale(x2)+offset, y+scale(y2), color);
  };
  const auto rect = [&](int16_t cx, int16_t cy, int16_t w, int16_t h, uint16_t color) {
    display.fillRect(x+scale(cx), y+scale(cy), scale(w), scale(h), color);
  };
  const auto horizontal = [&](int16_t cx, int16_t cy, int16_t w, uint16_t color) {
    display.fillRect(x+scale(cx), y+scale(cy), scale(w), stroke, color);
  };
  const auto vertical = [&](int16_t cx, int16_t cy, int16_t h, uint16_t color) {
    display.fillRect(x+scale(cx), y+scale(cy), stroke, scale(h), color);
  };
  const auto triangle = [&](int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                            int16_t x3, int16_t y3, uint16_t color) {
    display.fillTriangle(x+scale(x1), y+scale(y1), x+scale(x2), y+scale(y2),
                         x+scale(x3), y+scale(y3), color);
  };

  const auto sun = [&](int16_t cx, int16_t cy, int16_t radius) {
    circle(cx, cy, radius, TFT_YELLOW);
    const int16_t near = radius + 2;
    const int16_t far = radius + 4;
    line(cx, cy - near, cx, cy - far, TFT_YELLOW);
    line(cx, cy + near, cx, cy + far, TFT_YELLOW);
    line(cx - near, cy, cx - far, cy, TFT_YELLOW);
    line(cx + near, cy, cx + far, cy, TFT_YELLOW);
    for (int8_t dx : {-1, 1}) {
      for (int8_t dy : {-1, 1}) {
        line(cx + dx * (radius + 1), cy + dy * (radius + 1),
                         cx + dx * (radius + 3), cy + dy * (radius + 3),
                         TFT_YELLOW);
      }
    }
  };
  const auto cloud = [&] {
    circle(8, 12, 6, TFT_LIGHTGREY);
    circle(16, 10, 7, TFT_LIGHTGREY);
    circle(23, 13, 4, TFT_LIGHTGREY);
    rect(8, 12, 16, 6, TFT_LIGHTGREY);
  };

  if (weatherCode == 0) {
    sun(14, 14, 6);
    return;
  }
  if (weatherCode == 1 || weatherCode == 2) {
    sun(9, 9, 5);
    cloud();
    return;
  }
  if (weatherCode == 3) {
    cloud();
    return;
  }
  if (weatherCode == 45 || weatherCode == 48) {
    horizontal(4, 7, 20, TFT_LIGHTGREY);
    horizontal(1, 12, 24, TFT_LIGHTGREY);
    horizontal(4, 17, 22, TFT_LIGHTGREY);
    horizontal(2, 22, 19, TFT_LIGHTGREY);
    return;
  }
  switch (weatherCode) {
    case 51: case 53: case 55: case 56: case 57:  // drizzle/freezing drizzle
    case 61: case 63: case 65: case 66: case 67:  // rain/freezing rain
    case 80: case 81: case 82:                  // rain showers
      cloud();
      for (int16_t dx : {6, 14, 22}) {
        triangle(dx, 19, dx - 2, 23,
                             dx + 2, 23, TFT_CYAN);
        circle(dx, 24, 2, TFT_CYAN);
      }
      return;
    case 71: case 73: case 75: case 77: case 85: case 86:
      cloud();
      for (int16_t dx : {7, 20}) {
        vertical(dx, 19, 8, TFT_WHITE);
        line(dx - 3, 21, dx + 3, 25, TFT_WHITE);
        line(dx - 3, 25, dx + 3, 21, TFT_WHITE);
      }
      return;
    case 95: case 96: case 99:
      cloud();
      triangle(14, 17, 10, 23,
                           17, 22, TFT_YELLOW);
      triangle(14, 21, 18, 21,
                           11, 27, TFT_YELLOW);
      return;
    default:
      // Unknown codes get a neutral marker rather than a false forecast.
      outlineCircle(14, 14, 9, TFT_LIGHTGREY);
      vertical(14, 8, 8, TFT_LIGHTGREY);
      circle(14, 20, 1, TFT_LIGHTGREY);
      return;
  }
}

uint16_t dustGradeColor(DustGrade grade) {
  switch (grade) {
    case DustGrade::Good: return TFT_CYAN;
    case DustGrade::Normal: return TFT_GREEN;
    case DustGrade::Bad: return TFT_ORANGE;
    case DustGrade::VeryBad: return TFT_RED;
  }
  return TFT_LIGHTGREY;
}

const char *displayDustGradeName(DustGrade grade) {
  switch (grade) {
    case DustGrade::Good: return "GOOD";
    case DustGrade::Normal: return "OK";
    case DustGrade::Bad: return "BAD";
    case DustGrade::VeryBad: return "V.BAD";
  }
  return "--";
}

ForecastDate displayForecastDate(int &weekday) {
  weekday = -1;
  const time_t now = time(nullptr);
  if (now > 1000000000) {
    tm localNow{};
    localtime_r(&now, &localNow);
    weekday = localNow.tm_wday;
    return {localNow.tm_year + 1900, localNow.tm_mon + 1, localNow.tm_mday};
  }
  ForecastDate date;
  if (seoulWeatherValid && parseForecastDate(seoulWeather[0].date, date)) return date;
  if (airQualityValid && parseForecastDate(seoulAirQuality.days[0].date, date)) return date;
  return {};
}

// Baseline datums and transparent GFX text prevent adjacent text erasure.
uint8_t fitTextFont(const char *text, int16_t width, uint8_t maximum = 24) {
  uint8_t selected = maximum;
  display.setFreeFont(maximum == 48 ? &InterTightDigits48 : maximum == 36 ? &InterTightBold36 : &InterTightBold24);
  if (display.textWidth(text) > width && maximum == 48) { display.setFreeFont(&InterTightBold36); selected = 36; }
  if (display.textWidth(text) > width && maximum >= 36) { display.setFreeFont(&InterTightBold24); selected = 24; }
  if (display.textWidth(text) > width) { display.setFreeFont(&InterTightBold18); selected = 18; }
  if (display.textWidth(text) > width) { display.setFreeFont(&InterTightCompact13); selected = 13; }
  if (display.textWidth(text) > width) { display.setTextFont(1); selected = 6; }
  return selected;
}

void drawPageHeader() {
  display.fillScreen(TFT_BLACK);
  display.drawRoundRect(1, 1, 238, 238, 10, kDividerColor);
  char dateText[6] = "--/--", clockText[6] = "--:--";
  const char *weekday = "---";
  const time_t now = time(nullptr);
  if (now > 1000000000) {
    tm localNow{};
    localtime_r(&now, &localNow);
    snprintf(dateText, sizeof(dateText), "%02u/%02u", unsigned(localNow.tm_mon + 1) % 100, unsigned(localNow.tm_mday) % 100);
    snprintf(clockText, sizeof(clockText), "%02u:%02u", unsigned(localNow.tm_hour) % 24, unsigned(localNow.tm_min) % 60);
    weekday = displayWeekdayName(localNow.tm_wday, 0);
  }
  display.setTextColor(TFT_WHITE);
  display.setTextDatum(L_BASELINE);
  display.setFreeFont(&InterTightBold24);
  display.drawString(dateText, 4, 31);
  display.setTextDatum(R_BASELINE);
  display.setFreeFont(&InterTightDigits48);
  display.setTextColor(TFT_YELLOW);
  display.drawString(clockText, 236, 42);
  display.setTextDatum(C_BASELINE);
  // The 48px clock leaves a narrow centre lane.  The compact weekday keeps
  // all possible times (including 23:59) clear of the date and clock.
  display.setFreeFont(&InterTightCompact13);
  display.setTextColor(TFT_CYAN);
  display.drawString(weekday, 85, 29);
  display.drawFastHLine(59, 33, 42, TFT_CYAN);
  display.setFreeFont(&InterTightCompact13);
  // City gets its own small line; it never reduces the date or time font.
  char cityText[21];
  strlcpy(cityText, citySettings.name, sizeof(cityText));
  while (display.textWidth(cityText) > 42 && cityText[0]) cityText[strlen(cityText)-1] = '\0';
  display.drawString(cityText, 80, 47);
  display.drawFastHLine(6, 52, 228, kDividerColor);
}

void drawDroplet(int16_t x, int16_t y, int16_t size) {
  display.fillTriangle(x + size/2, y, x, y + size*2/3, x + size-1, y + size*2/3, TFT_CYAN);
  display.fillCircle(x + size/2, y + size*2/3, size/3, TFT_CYAN);
}

void drawTemperature(float value, bool valid, int16_t right, int16_t baseline,
                     uint8_t maximum, int16_t width, uint16_t color) {
  char number[16] = "--";
  if (valid) snprintf(number, sizeof(number), "%d", static_cast<int>(roundf(value)));
  display.setTextColor(color);
  display.setTextDatum(R_BASELINE);
  const uint8_t size = fitTextFont(number, width, maximum);
  display.drawString(number, right, baseline);
  // The digits-only 48px font uses a tiny vector degree mark.
  const int16_t radius = size >= 36 ? 3 : 2;
  display.drawCircle(right + (maximum >= 36 ? 8 : 5), baseline - size*3/4 + radius,
                     radius, color);
}

void drawRain(const SeoulWeatherDay *weather, int16_t x, int16_t y, bool today) {
  char percent[8] = "--%";
  if (weather) snprintf(percent, sizeof(percent), "%d%%", weather->rainProbability);
  display.setTextColor(TFT_CYAN);
  if (today) {
    // TODAY uses a dedicated precipitation column: icon above, value below.
    drawDroplet(x, y, 24);
    display.setTextDatum(C_BASELINE);
    fitTextFont(percent, 38, 24);
    display.drawString(percent, x + 12, y + 61);
  } else {
    fitTextFont(percent, 38, 18);
    display.setTextDatum(R_BASELINE);
    display.drawString(percent, 234, y + 11);
  }
}

void drawAirQualityFace(int16_t x, int16_t y, int16_t radius, DustGrade grade, bool valid) {
  const uint16_t color = valid ? dustGradeColor(grade) : TFT_LIGHTGREY;
  display.fillCircle(x, y, radius, color);
  const int16_t eyeY = y - radius/5;
  const int16_t eyeRadius = radius >= 17 ? 2 : 1;
  display.fillCircle(x - radius/3, eyeY, eyeRadius, TFT_BLACK);
  display.fillCircle(x + radius/3, eyeY, eyeRadius, TFT_BLACK);
  if (grade == DustGrade::Bad || grade == DustGrade::VeryBad) {
    const int16_t mouthY = y + radius/3;
    if (grade == DustGrade::Bad) {
      display.drawFastHLine(x - radius/3, mouthY, radius*2/3 + 1, TFT_BLACK);
    } else {
      display.drawLine(x - radius/3, mouthY + 2, x, mouthY - 1, TFT_BLACK);
      display.drawLine(x, mouthY - 1, x + radius/3, mouthY + 2, TFT_BLACK);
    }
  } else {
    const int16_t mouthY = y + radius/4;
    display.drawLine(x - radius/3, mouthY - 1, x, mouthY + 2, TFT_BLACK);
    display.drawLine(x, mouthY + 2, x + radius/3, mouthY - 1, TFT_BLACK);
  }
}

void drawGradeBadge(int16_t center, int16_t top, int16_t width,
                    DustGrade grade, bool valid, int16_t height = 20) {
  const uint16_t color = valid ? dustGradeColor(grade) : TFT_LIGHTGREY;
  const char *label = valid
      ? (grade == DustGrade::Normal ? "NORMAL" : displayDustGradeName(grade))
      : "--";
  display.fillRoundRect(center - width/2, top, width, height,
                        height >= 20 ? 6 : 4, color);
  display.setTextColor(TFT_BLACK);
  display.setTextDatum(C_BASELINE);
  fitTextFont(label, width - 6, height >= 20 ? 18 : 13);
  display.drawString(label, center, top + height - (height >= 20 ? 4 : 3));
}

void drawWeatherPage() {
  drawPageHeader();
  if (!seoulWeatherValid) {
    display.setTextDatum(MC_DATUM);
    display.setFreeFont(&InterTightBold18);
    display.setTextColor(TFT_YELLOW);
    display.drawString("WEATHER NOT READY", 120, 112);
    display.setTextColor(TFT_WHITE);
    const String error = String(seoulWeatherErrorName(seoulWeatherLastError)) +
                         " " + String(seoulWeatherLastHttpCode);
    display.drawString(error, 120, 139);
    const String detail = seoulWeatherLastErrorText.substring(0, 30);
    fitTextFont(detail.c_str(), 224);
    display.setTextColor(TFT_LIGHTGREY);
    display.drawString(detail, 120, 165);
    return;
  }
  int weekday;
  ForecastDate date = displayForecastDate(weekday);
  for (uint8_t i=0; i<3; ++i) {
    char iso[11]; formatForecastDate(iso, date);
    const SeoulWeatherDay *weather = nullptr;
    for (const auto &day : seoulWeather) {
      if (!strcmp(day.date,iso)) { weather=&day; break; }
    }
    if (i == 0) {
      drawWeatherIcon(4, 60, weather ? weather->weatherCode : -1, 84);
      drawTemperature(weather ? weather->high : 0, weather, 178, 105, 48, 78, TFT_ORANGE);
      drawTemperature(weather ? weather->low : 0, weather, 178, 142, 36, 78, TFT_CYAN);
      display.drawFastVLine(190, 61, 82, kDividerColor);
      drawRain(weather, 204, 69, true);
      display.drawFastHLine(6, 148, 228, kDividerColor);
    } else {
      const int16_t row = i == 1 ? 151 : 195;
      display.setFreeFont(&InterTightBold24);
      display.setTextColor(TFT_WHITE);
      display.setTextDatum(L_BASELINE);
      display.drawString(displayWeekdayName(weekday, i), 5, row+30);
      drawWeatherIcon(62, row+3, weather ? weather->weatherCode : -1, 36);
      drawTemperature(weather ? weather->high : 0, weather, 144, row+30, 36, 42, TFT_WHITE);
      drawTemperature(weather ? weather->low : 0, weather, 187, row+30, 24, 34, TFT_CYAN);
      display.drawFastVLine(195, row+4, 36, kDividerColor);
      drawRain(weather, 0, row+19, false);
      if (i == 1) display.drawFastHLine(6, 194, 228, kDividerColor);
    }
    if (date.year) date=nextForecastDate(date);
  }
}

void drawAirQualityPage() {
  drawPageHeader();
  display.setTextDatum(C_BASELINE);
  display.setFreeFont(&InterTightBold18);
  display.setTextColor(TFT_LIGHTGREY);
  display.drawString("PM2.5",60, 76);
  display.drawString("PM10",180, 76);
  display.drawFastVLine(120, 61, 83, kDividerColor);
  display.drawFastHLine(6, 148, 228, kDividerColor);
  int weekday;
  ForecastDate date=displayForecastDate(weekday);
  for (uint8_t i=0; i<3; ++i) {
    const int16_t row = i == 1 ? 153 : 195;
    char iso[11]; formatForecastDate(iso,date);
    const SeoulAirQualityDay *reading=nullptr;
    if (airQualityValid) {
      for (const auto &day : seoulAirQuality.days) {
        if (!strcmp(day.date,iso)) { reading=&day; break; }
      }
    }
    if (i != 0) {
      display.setTextDatum(L_BASELINE);
      display.setFreeFont(&InterTightBold24);
      display.setTextColor(TFT_WHITE);
      display.drawString(displayWeekdayName(weekday, i), 4, row+29);
    }
    for (uint8_t column=0; column<2; ++column) {
      const bool valid=reading && (column==0 ? reading->pm25Valid : reading->pm10Valid);
      const int value=reading ? (column==0 ? reading->pm25 : reading->pm10) : 0;
      const DustGrade grade=dustGrade(value,column==0);
      const int16_t x = i == 0 ? (column == 0 ? 48 : 199)
                               : (column == 0 ? 134 : 207);
      display.setTextColor(valid ? dustGradeColor(grade) : TFT_LIGHTGREY);
      display.setTextDatum(C_BASELINE);
      char number[12]="--";
      if (valid) {
        if (value > 999) strlcpy(number, "999", sizeof(number));
        else snprintf(number,sizeof(number),"%d",value);
      }
      fitTextFont(number, i == 0 ? 62 : 48, i == 0 ? 48 : 36);
      display.drawString(number, x, i == 0 ? 121 : row+26);
      if (i == 0) {
        drawAirQualityFace(column == 0 ? 100 : 143, 104, column == 0 ? 18 : 17, grade, valid);
        drawGradeBadge(column == 0 ? 60 : 180, 125, 100, grade, valid);
      } else {
        if (column == 0) drawAirQualityFace(86, row + 18, 14, grade, valid);
        drawGradeBadge(column == 0 ? 136 : 205, row+29,
                       column == 0 ? 60 : 60, grade, valid, 15);
      }
    }

    if (i == 1) {
      display.drawFastVLine(168, row+3, 38, kDividerColor);
      display.drawFastHLine(6, 194, 228, kDividerColor);
    } else if (i == 2) {
      display.drawFastVLine(168, row+3, 38, kDividerColor);
    }

    if (date.year) date=nextForecastDate(date);
  }
}

#endif

// Rendering and page timing never initiate a network request.
void drawCurrentPage() {
#ifdef SDPRO_TICKER
  tickerDraw(display);
#else
  if (pageCycle.page==ForecastPage::Weather) {
    drawWeatherPage();
    weatherDirty = false;
  } else {
    drawAirQualityPage();
    airQualityDirty = false;
  }
#endif
  drawnPageMinute = time(nullptr) / 60;
  lastDrawAt=millis();
}


}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\n%s SD PRO ESP8266\n", kVersion);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event) {
    lastWifiDisconnectReason = event.reason;
    Serial.printf("Wi-Fi disconnect reason: %u\n", event.reason);
  });
  loadDeviceConfig(config);
  filesystemReady = LittleFS.begin();
  loadNetworkSettings(networkSettings, filesystemReady);
#ifdef SDPRO_TICKER
  tickerLoad(filesystemReady);
#endif
  loadDisplaySettings(displaySettings, filesystemReady);
  pageCycle.configure(millis(), displaySettings.autoRotate, displaySettings.weatherPageSeconds,
                      displaySettings.airPageSeconds, displaySettings.fixedPage);
  loadCitySettings(citySettings, filesystemReady);
  display.init();
  display.setRotation(0);
  display.setTextWrap(false, false);
  applyBacklight();
  message(citySettings.name, "Starting...");
  configureRoutes();
  connectToWiFi();
}

void loop() {
  server.handleClient();
  if (accessPointRunning) dns.processNextRequest();
  const uint32_t now = millis();
  if (restartAt && static_cast<int32_t>(now - restartAt) >= 0) ESP.restart();
  // Short cooperative idle keeps HTTP/DNS serviced without spinning on timers.
  if (updating || restartAt || now - lastServiceAt < kServiceIntervalMs) {
    delay(10);
    return;
  }
  lastServiceAt = now;
  applyBacklight();
  const int wifiStatus = WiFi.status();
  if (wifiStatus != lastWifiStatus) {
    lastWifiStatus = wifiStatus;
    Serial.printf("Wi-Fi status: %s (%d)\n", wifiStatusText(wifiStatus), wifiStatus);
    if (accessPointRunning) drawRecoveryWifiStatus(wifiStatus);
  }
  const bool connected = wifiStatus == WL_CONNECTED;
  if (pagesStarted && (!accessPointRunning || connected)) {
#ifdef SDPRO_TICKER
    if (tickerAdvance(now) || !lastDrawAt) drawCurrentPage();
#else
    const bool pageChanged = pageCycle.advance(now);
    const bool dirty = pageCycle.page == ForecastPage::Weather
        ? weatherDirty : airQualityDirty;
    if (pageChanged || dirty || time(nullptr) / 60 != drawnPageMinute || !lastDrawAt) drawCurrentPage();
#endif
  }
  if (!connected) {
    timeConfigured = false;
    if (bootIpShown && !pagesStarted) {
      bootIpShown = false;
      message("Connecting Wi-Fi", config.ssid);
    }
    const uint8_t limit = config.wifiRetryLimit ? config.wifiRetryLimit : kDefaultWifiRetryLimit;
    const auto retry = wifiRetryAction(now, connectStartedAt, wifiConfigured(config),
                                      wifiAttemptCount, limit, accessPointRunning);
    if (retry != WifiRetryAction::None) {
      if (retry == WifiRetryAction::StartRecovery) startAccessPoint();
      if (retry == WifiRetryAction::Retry || !accessPointRunning) {
        if (wifiAttemptCount < limit) ++wifiAttemptCount;
        // Retain credentials and the recovery AP when restarting station mode.
        WiFi.disconnect(false, false);
        WiFi.begin(config.ssid, config.wifiPassword);
        if (accessPointRunning) drawRecoveryWifiStatus(WiFi.status());
        else message("Retrying Wi-Fi", config.ssid);
      }
      connectStartedAt = now;
    }
    delay(10);
    return;
  }
  if (accessPointRunning) {
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    accessPointRunning = false;
    weatherDirty = airQualityDirty = true;
  }
  wifiAttemptCount = 0;
  // A later outage gets its own connection grace period, independent of uptime.
  connectStartedAt = now;
  if (!pagesStarted) {
    if (!bootIpShown) {
      message("Wi-Fi connected", WiFi.localIP().toString());
      bootIpShownAt = now;
      bootIpShown = true;
    }
    // Keep the address readable before rendering or a blocking API fetch.
    // HTTP, OTA and recovery servicing continue at the top of each loop.
    if (now - bootIpShownAt < kBootIpDisplayMs) {
      delay(10);
      return;
    }
    pagesStarted = true;
    pageCycle.begin(now);
    drawCurrentPage();
  }
  if (!timeConfigured) {
    configureTimeService(networkSettings, kDefaultTimezone);
    timeConfigured = true;
  }
#ifdef SDPRO_TICKER
  if (tickerPoll(now)) drawCurrentPage();
#else
  if (weatherRefresh.due(now)) {
    SeoulWeatherDay next[3]{};
    const bool success = fetchSeoulWeather(next, citySettings);
    weatherRefresh.complete(millis(), success);
    if (success) {
      memcpy(seoulWeather, next, sizeof(seoulWeather));
      seoulWeatherValid = true;
    }
    // Failed requests redraw diagnostics only while there is no cached forecast.
    weatherDirty = success || !seoulWeatherValid;
    if (pageCycle.page == ForecastPage::Weather && weatherDirty) drawCurrentPage();
    delay(10);
    return;  // Service HTTP before starting the other HTTPS request.
  }
  if (airQualityRefresh.due(now)) {
    SeoulAirQuality next{};
    const bool success = fetchSeoulAirQuality(next, citySettings);
    airQualityRefresh.complete(millis(), success);
    if (success) {
      seoulAirQuality = next;
      airQualityValid = true;
      airQualityDirty = true;
      if (pageCycle.page == ForecastPage::AirQuality) drawCurrentPage();
    }
  }
#endif
  delay(10);
}
