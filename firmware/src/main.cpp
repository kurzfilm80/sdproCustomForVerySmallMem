#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
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
#include "fonts/InterTightBold18.h"
#include "fonts/InterTightBold24.h"
#include "fonts/InterTightBold36.h"
#include "fonts/InterTightCompact13.h"
#include "WebAssets.generated.h"

namespace {
constexpr char kVersion[] = "SeoulWeather-4.0.0";
constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr uint32_t kWeatherRefreshMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryMs = 60000;
constexpr uint32_t kAirQualityRefreshMs = 15UL * 60UL * 1000UL;
DeviceConfig config{};
NetworkSettings networkSettings{};
ESP8266WebServer server(80);
DNSServer dns;
TFT_eSPI display;
SeoulWeatherDay seoulWeather[3]{};
bool seoulWeatherValid = false;
SeoulAirQuality seoulAirQuality{};
bool airQualityValid = false;
bool airQualityAttempted = false;
uint32_t lastAirQualityFetchAt = 0;
bool filesystemReady = false;
bool accessPointRunning = false;
bool mdnsReady = false;
bool timeConfigured = false;
bool weatherAttempted = false;
bool lastFetchSucceeded = false;
bool updating = false;
bool uploadStarted = false;
bool uploadSucceeded = false;
uint8_t wifiAttemptCount = 0;
uint8_t displayBrightness = 100;
uint32_t connectStartedAt = 0;
uint32_t lastFetchAt = 0;
uint32_t lastDrawAt = 0;
uint32_t restartAt = 0;
PageCycle pageCycle;
bool pagesStarted = false;

void applyBacklight() {
  pinMode(TFT_BL, OUTPUT);
  analogWriteRange(100);
  analogWrite(TFT_BL, TFT_BACKLIGHT_ON == LOW ? 100 - displayBrightness
                                            : displayBrightness);
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

// Use a separate document; the former dashboard display.json remains intact.
bool saveBrightness(uint8_t brightness) {
  if (!filesystemReady) return false;
  File file = LittleFS.open("/weather-settings.tmp", "w");
  if (!file) return false;
  StaticJsonDocument<128> doc;
  doc["schemaVersion"] = 1;
  doc["brightness"] = brightness;
  const bool written = serializeJson(doc, file) > 0;
  file.close();
  if (!written) return false;
  return LittleFS.rename("/weather-settings.tmp", "/weather-settings.json");
}

void loadBrightness() {
  if (!filesystemReady) return;
  const bool dedicated = LittleFS.exists("/weather-settings.json");
  File file = LittleFS.open(dedicated ? "/weather-settings.json" : "/display.json", "r");
  if (!file) return;
  StaticJsonDocument<1024> doc;
  const auto error = deserializeJson(doc, file);
  file.close();
  const auto schema = inspectStoredConfigSchema(doc.as<JsonObjectConst>(), 1);
  if (error || !doc.is<JsonObject>() ||
      schema.state != StoredConfigSchemaState::Current) {
    if (dedicated) {
      quarantineStoredConfigFile("/weather-settings.json", "/weather-settings.invalid");
      saveBrightness(100);
    }
    return;
  }
  if (doc["brightness"].is<int>() && doc["brightness"].as<int>() >= 0 &&
      doc["brightness"].as<int>() <= 100) {
    displayBrightness = doc["brightness"].as<uint8_t>();
  } else if (dedicated) {
    saveBrightness(100);
  }
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
  Serial.printf("Setup portal: http://%s/\n", WiFi.softAPIP().toString().c_str());
}

void connectToWiFi() {
  if (!wifiConfigured(config)) { startAccessPoint(); return; }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
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
  StaticJsonDocument<768> doc;
  doc["version"] = kVersion;
  doc["ssid"] = config.ssid;
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
  saveDeviceConfig(next);
  config = next;
  server.send(200, "text/plain", "Settings saved. Restarting...");
  restartAt = millis() + 500;
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
  if (!saveBrightness(value)) { server.send(500, "text/plain", "Could not save brightness"); return; }
  displayBrightness = value;
  applyBacklight();
  server.sendHeader("Location", "/");
  server.send(303);
}

void resumeAfterUpdateFailure() {
  updating = false;
  timeConfigured = false;
  lastDrawAt = 0;
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
  server.on("/", HTTP_GET, sendPage);
  server.on("/network", HTTP_GET, sendPage);
  server.on("/update", HTTP_GET, sendPage);
  server.on("/settings", HTTP_POST, saveSettings);
  server.on("/brightness", HTTP_POST, setBrightness);
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

// Scale the existing primitive icon to 36x36 without a sprite or bitmap.
void drawWeatherIcon(int16_t x, int16_t y, int weatherCode) {
  constexpr int16_t size = 36;
  const auto scale = [](int16_t v) -> int16_t { return (v * size + 14) / 28; };
  display.fillRect(x, y, size, size, TFT_BLACK);
  const auto circle = [&](int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    display.fillCircle(x+scale(cx), y+scale(cy), scale(r), color);
  };
  const auto outlineCircle = [&](int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    display.drawCircle(x+scale(cx), y+scale(cy), scale(r), color);
  };
  const auto line = [&](int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    display.drawLine(x+scale(x1), y+scale(y1), x+scale(x2), y+scale(y2), color);
  };
  const auto rect = [&](int16_t cx, int16_t cy, int16_t w, int16_t h, uint16_t color) {
    display.fillRect(x+scale(cx), y+scale(cy), scale(w), scale(h), color);
  };
  const auto horizontal = [&](int16_t cx, int16_t cy, int16_t w, uint16_t color) {
    display.drawFastHLine(x+scale(cx), y+scale(cy), scale(w), color);
  };
  const auto vertical = [&](int16_t cx, int16_t cy, int16_t h, uint16_t color) {
    display.drawFastVLine(x+scale(cx), y+scale(cy), scale(h), color);
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
      outlineCircle(14, 14, 9, TFT_DARKGREY);
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
  return TFT_DARKGREY;
}

ForecastDate displayForecastDate() {
  const time_t now = time(nullptr);
  if (now > 1000000000) {
    tm localNow{};
    localtime_r(&now, &localNow);
    return {localNow.tm_year + 1900, localNow.tm_mon + 1, localNow.tm_mday};
  }
  ForecastDate date;
  if (seoulWeatherValid && parseForecastDate(seoulWeather[0].date, date)) return date;
  if (airQualityValid && parseForecastDate(seoulAirQuality.days[0].date, date)) return date;
  return {};
}

const char *dayLabel(uint8_t index, ForecastDate date) {
  if (index == 0) return "TODAY";
  if (index == 1) return "TOMORROW";
  return date.year ? forecastWeekdayName(date) : "---";
}

void drawPageHeader(const char *title) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.setFreeFont(&InterTightBold24);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(title, 120, 20);
  char timeLine[32] = "--/-- ---  --:--";
  const time_t now = time(nullptr);
  if (now > 1000000000) {
    tm localNow{};
    localtime_r(&now, &localNow);
    const ForecastDate date{localNow.tm_year+1900, localNow.tm_mon+1, localNow.tm_mday};
    snprintf(timeLine, sizeof(timeLine), "%02d/%02d %s  %02d:%02d",
             date.month, date.day, forecastWeekdayName(date), localNow.tm_hour, localNow.tm_min);
  }
  display.setFreeFont(&InterTightBold18);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(timeLine, 120, 47);
}

void drawWeatherPage() {
  drawPageHeader("SEOUL");
  if (!seoulWeatherValid) {
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.drawString("WEATHER NOT READY", 120, 112);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    const String error = String(seoulWeatherErrorName(seoulWeatherLastError)) +
                         " " + String(seoulWeatherLastHttpCode);
    display.drawString(error, 120, 139);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.drawString(seoulWeatherLastErrorText.substring(0, 30), 120, 165);
    return;
  }
  ForecastDate date = displayForecastDate();
  for (uint8_t i=0; i<3; ++i) {
    const int16_t top = 65 + i*58;
    char iso[11]; formatForecastDate(iso, date);
    const SeoulWeatherDay *weather = nullptr;
    for (const auto &day : seoulWeather) {
      if (!strcmp(day.date,iso)) { weather=&day; break; }
    }
    display.setTextDatum(ML_DATUM);
    display.setFreeFont(&InterTightBold18);
    display.setTextColor(i==0 ? TFT_YELLOW : TFT_CYAN, TFT_BLACK);
    display.drawString(dayLabel(i,date), 8, top+8);
    drawWeatherIcon(8, top+20, weather ? weather->weatherCode : -1);
    char temperature[32] = "-- / -- C";
    char rain[20] = "RAIN --%";
    if (weather) {
      snprintf(temperature,sizeof(temperature),"%d / %d C",
               static_cast<int>(roundf(weather->high)), static_cast<int>(roundf(weather->low)));
      snprintf(rain,sizeof(rain),"RAIN %d%%",weather->rainProbability);
    }
    display.setTextDatum(MR_DATUM);
    display.setTextColor(TFT_WHITE,TFT_BLACK);
    display.setFreeFont(&InterTightBold24);
    if (display.textWidth(temperature)>108) display.setFreeFont(&InterTightBold18);
    display.drawString(temperature,232,top+14);
    display.setFreeFont(&InterTightBold18);
    display.setTextColor(TFT_LIGHTGREY,TFT_BLACK);
    display.drawString(rain,232,top+42);
    if (i<2) display.drawFastHLine(8,top+57,224,TFT_DARKGREY);
    if (date.year) date=nextForecastDate(date);
  }
}

void drawAirQualityPage() {
  drawPageHeader("SEOUL AIR");
  display.setFreeFont(&InterTightBold18);
  display.setTextColor(TFT_LIGHTGREY,TFT_BLACK);
  display.drawString("PM2.5",140,72);
  display.drawString("PM10",210,72);
  ForecastDate date=displayForecastDate();
  for (uint8_t i=0; i<3; ++i) {
    const int16_t y=99+i*54;
    char iso[11]; formatForecastDate(iso,date);
    const SeoulAirQualityDay *reading=nullptr;
    if (airQualityValid) {
      for (const auto &day : seoulAirQuality.days) {
        if (!strcmp(day.date,iso)) { reading=&day; break; }
      }
    }
    display.setFreeFont(&InterTightBold18);
    display.setTextDatum(ML_DATUM);
    display.setTextColor(i==0 ? TFT_YELLOW : TFT_CYAN,TFT_BLACK);
    display.drawString(dayLabel(i,date),4,y+5);
    for (uint8_t column=0; column<2; ++column) {
      const bool valid=reading && (column==0 ? reading->pm25Valid : reading->pm10Valid);
      const int value=reading ? (column==0 ? reading->pm25 : reading->pm10) : 0;
      const DustGrade grade=dustGrade(value,column==0);
      const int16_t x=column==0 ? 140 : 210;
      display.setTextColor(valid ? dustGradeColor(grade) : TFT_DARKGREY,TFT_BLACK);
      display.setTextDatum(MC_DATUM);
      char number[12]="--";
      if (valid) snprintf(number,sizeof(number),"%d",value);
      display.setFreeFont(&InterTightBold36);
      if (display.textWidth(number)>64) display.setFreeFont(&InterTightBold24);
      if (display.textWidth(number)>64) display.setFreeFont(&InterTightBold18);
      if (display.textWidth(number)>64) display.setTextFont(1);
      display.drawString(number,x,y);
      display.setFreeFont(&InterTightCompact13);
      display.drawString(valid ? dustGradeName(grade) : "--",x,y+25);
    }
    if (i<2) display.drawFastHLine(4,y+35,232,TFT_DARKGREY);
    if (date.year) date=nextForecastDate(date);
  }
}

// Rendering and page timing never initiate a network request.
void drawCurrentPage() {
  if (pageCycle.page==ForecastPage::Weather) drawWeatherPage();
  else drawAirQualityPage();
  lastDrawAt=millis();
}


}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\n%s SD PRO ESP8266\n", kVersion);
  loadDeviceConfig(config);
  filesystemReady = LittleFS.begin();
  loadNetworkSettings(networkSettings, filesystemReady);
  loadBrightness();
  display.init();
  display.setRotation(0);
  display.setTextWrap(false, false);
  applyBacklight();
  message("SEOUL WEATHER", "Starting...");
  configureRoutes();
  connectToWiFi();
}

void loop() {
  server.handleClient();
  if (accessPointRunning) dns.processNextRequest();
  if (restartAt && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
  if (updating || restartAt) { delay(2); return; }
  if (pagesStarted && (!accessPointRunning || WiFi.status()==WL_CONNECTED)) {
    if (pageCycle.advance(millis()) || !lastDrawAt || millis()-lastDrawAt>=30000)
      drawCurrentPage();
  }
  if (WiFi.status() != WL_CONNECTED) {
    timeConfigured = false;
    if (mdnsReady) { MDNS.close(); mdnsReady = false; }
    if (wifiConfigured(config) && millis() - connectStartedAt >= kConnectTimeoutMs) {
      const uint8_t limit = config.wifiRetryLimit ? config.wifiRetryLimit : kDefaultWifiRetryLimit;
      if (wifiAttemptCount >= limit) startAccessPoint();
      else {
        ++wifiAttemptCount;
        WiFi.disconnect();
        WiFi.begin(config.ssid, config.wifiPassword);
        message("Retrying Wi-Fi", config.ssid);
      }
      connectStartedAt = millis();
    }
    delay(2);
    return;
  }
  wifiAttemptCount = 0;
  if (!pagesStarted) {
    pagesStarted=true;
    pageCycle.begin(millis());
    drawCurrentPage();
  }
  if (!timeConfigured) {
    configureTimeService(networkSettings, kDefaultTimezone);
    timeConfigured = true;
    lastDrawAt = 0;
  }
  if (!mdnsReady) {
    mdnsReady = MDNS.begin(configuredHostname(config).c_str());
    if (mdnsReady) MDNS.addService("http", "tcp", 80);
  }
  if (mdnsReady) MDNS.update();
  const uint32_t interval = lastFetchSucceeded ? kWeatherRefreshMs : kWeatherRetryMs;
  if (!weatherAttempted || millis() - lastFetchAt >= interval) {
    weatherAttempted = true;
    SeoulWeatherDay next[3]{};
    lastFetchSucceeded = fetchSeoulWeather(next);
    // Count from completion so a slow failure still leaves a full retry pause.
    lastFetchAt = millis();
    if (lastFetchSucceeded) {
      memcpy(seoulWeather, next, sizeof(seoulWeather));
      seoulWeatherValid = true;
    }
    drawCurrentPage();
    // Return to the web server before starting the other HTTPS request.
    delay(2);
    return;
  }
  if (!airQualityAttempted || millis() - lastAirQualityFetchAt >= kAirQualityRefreshMs) {
    airQualityAttempted = true;
    SeoulAirQuality next{};
    airQualityValid = fetchSeoulAirQuality(next);
    lastAirQualityFetchAt = millis();
    if (airQualityValid) seoulAirQuality = next;
    drawCurrentPage();
  }
  delay(2);
}
