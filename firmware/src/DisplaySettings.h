#pragma once
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include "PageCycle.h"
#include "StoredConfig.h"

struct DisplaySettings {
  bool autoRotate = true;
  uint8_t weatherPageSeconds = 15;
  uint8_t airPageSeconds = 10;
  ForecastPage fixedPage = ForecastPage::Weather;
  uint8_t brightness = 80;
  bool nightModeEnabled = false;
  uint16_t nightStart = 22 * 60;
  uint16_t nightEnd = 7 * 60;
  uint8_t nightBrightness = 30;
};

inline bool pageSecondsValid(unsigned value) {
  return value == 5 || value == 10 || value == 15 || value == 30 || value == 60;
}
inline bool nightBrightnessValid(unsigned value) {
  return value == 20 || value == 30 || value == 40 || value == 50;
}
inline bool parseDisplayTime(const char *text, uint16_t &minutes) {
  if (!text || std::strlen(text) != 5 || text[2] != ':') return false;
  for (unsigned i = 0; i < 5; ++i)
    if (i != 2 && (text[i] < '0' || text[i] > '9')) return false;
  const unsigned hour = (text[0] - '0') * 10 + text[1] - '0';
  const unsigned minute = (text[3] - '0') * 10 + text[4] - '0';
  if (hour > 23 || minute > 59) return false;
  minutes = hour * 60 + minute;
  return true;
}
inline void formatDisplayTime(uint16_t minutes, char (&text)[6]) {
  std::snprintf(text, sizeof(text), "%02u:%02u", unsigned(minutes / 60 % 24), unsigned(minutes % 60));
}
// Equal start/end means an empty interval. Unsynchronized time uses normal light.
inline uint8_t effectiveDisplayBrightness(const DisplaySettings &settings,
                                         uint16_t minute, bool timeValid) {
  if (!settings.nightModeEnabled || !timeValid) return settings.brightness;
  const bool night = settings.nightStart <= settings.nightEnd
      ? minute >= settings.nightStart && minute < settings.nightEnd
      : minute >= settings.nightStart || minute < settings.nightEnd;
  return night ? settings.nightBrightness : settings.brightness;
}

// Additive schema 1: retain the original brightness range for existing files/API.
inline bool decodeDisplaySettings(JsonObjectConst document, DisplaySettings &settings,
                                  bool &repaired) {
  settings = DisplaySettings{};
  const auto schema = inspectStoredConfigSchema(document, 1);
  if (schema.state != StoredConfigSchemaState::Current) return false;
  repaired = schema.legacy;
  settings.autoRotate = readStoredBool(document, "autoRotate", true, repaired);
  settings.weatherPageSeconds = readStoredInt(document, "weatherPageSeconds", 15, 5, 60, repaired);
  if (!pageSecondsValid(settings.weatherPageSeconds)) { settings.weatherPageSeconds = 15; repaired = true; }
  settings.airPageSeconds = readStoredInt(document, "airPageSeconds", 10, 5, 60, repaired);
  if (!pageSecondsValid(settings.airPageSeconds)) { settings.airPageSeconds = 10; repaired = true; }
  const char *fixed = document["fixedPage"] | "";
  if (!strcmp(fixed, "AIR QUALITY")) settings.fixedPage = ForecastPage::AirQuality;
  else if (strcmp(fixed, "WEATHER")) repaired = true;
  settings.brightness = readStoredInt(document, "brightness", 80, 0, 100, repaired);
  settings.nightModeEnabled = readStoredBool(document, "nightModeEnabled", false, repaired);
  if (!parseDisplayTime(document["nightStart"] | "", settings.nightStart)) repaired = true;
  if (!parseDisplayTime(document["nightEnd"] | "", settings.nightEnd)) repaired = true;
  settings.nightBrightness = readStoredInt(document, "nightBrightness", 30, 20, 50, repaired);
  if (!nightBrightnessValid(settings.nightBrightness)) { settings.nightBrightness = 30; repaired = true; }
  return true;
}
inline void encodeDisplaySettings(JsonObject document, const DisplaySettings &settings) {
  document["schemaVersion"] = 1;
  document["autoRotate"] = settings.autoRotate;
  document["weatherPageSeconds"] = settings.weatherPageSeconds;
  document["airPageSeconds"] = settings.airPageSeconds;
  document["fixedPage"] = settings.fixedPage == ForecastPage::Weather ? "WEATHER" : "AIR QUALITY";
  document["brightness"] = settings.brightness;
  document["nightModeEnabled"] = settings.nightModeEnabled;
  char start[6], end[6];
  formatDisplayTime(settings.nightStart, start);
  formatDisplayTime(settings.nightEnd, end);
  document["nightStart"] = start;
  document["nightEnd"] = end;
  document["nightBrightness"] = settings.nightBrightness;
}

bool saveDisplaySettings(const DisplaySettings &settings, bool filesystemReady);
void loadDisplaySettings(DisplaySettings &settings, bool filesystemReady);
