#include "DisplaySettings.h"
#include <Arduino.h>
#include <LittleFS.h>
#include "StoredConfigFile.h"

namespace {
constexpr char kPath[] = "/weather-settings.json";
constexpr char kTemporaryPath[] = "/weather-settings.tmp";
}

bool saveDisplaySettings(const DisplaySettings &settings, bool filesystemReady) {
  if (!filesystemReady) return false;
  StaticJsonDocument<512> document;
  encodeDisplaySettings(document.to<JsonObject>(), settings);
  if (document.overflowed()) return false;
  File file = LittleFS.open(kTemporaryPath, "w");
  if (!file) return false;
  const bool written = serializeJson(document, file) == measureJson(document);
  file.close();
  if (!written || !LittleFS.rename(kTemporaryPath, kPath)) {
    LittleFS.remove(kTemporaryPath);
    return false;
  }
  return true;
}

void loadDisplaySettings(DisplaySettings &settings, bool filesystemReady) {
  settings = DisplaySettings{};
  if (!filesystemReady) return;
  const bool dedicated = LittleFS.exists(kPath);
  const char *path = dedicated ? kPath : "/display.json";
  if (!LittleFS.exists(path)) return;
  File file = LittleFS.open(path, "r");
  StaticJsonDocument<1024> document;
  // Ignore former dashboard settings; only import its validated brightness.
  StaticJsonDocument<96> filter;
  filter["schemaVersion"] = true;
  filter["brightness"] = true;
  const bool malformed = !file || (dedicated && file.size() > 1024) ||
      (dedicated ? deserializeJson(document, file)
                 : deserializeJson(document, file, DeserializationOption::Filter(filter))) ||
      !document.is<JsonObject>();
  file.close();
  bool repaired = false;
  if (malformed || !decodeDisplaySettings(document.as<JsonObjectConst>(), settings, repaired)) {
    settings = DisplaySettings{};
    Serial.println(F("Display settings: malformed or unsupported; using defaults"));
    if (dedicated) quarantineStoredConfigFile(kPath, "/weather-settings.invalid");
    else return;  // Never modify the former dashboard document.
    repaired = true;
  }
  if (repaired || !dedicated) {
    Serial.println(F("Display settings: normalizing defaults and legacy fields"));
    if (!saveDisplaySettings(settings, filesystemReady))
      Serial.println(F("Display settings: could not persist normalization"));
  }
}
