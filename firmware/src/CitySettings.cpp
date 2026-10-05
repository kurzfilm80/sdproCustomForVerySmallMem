#include "CitySettings.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "StoredConfig.h"
#include "StoredConfigFile.h"

namespace {
struct PresetCity { char name[9]; char latitude[9]; char longitude[10]; };
const PresetCity kCities[] PROGMEM = {
  {"SEOUL", "37.5665", "126.9780"},
  {"BUSAN", "35.1796", "129.0756"},
  {"INCHEON", "37.4563", "126.7052"},
  {"DAEJEON", "36.3504", "127.3845"},
  {"DAEGU", "35.8714", "128.6014"},
  {"GWANGJU", "35.1595", "126.8526"},
  {"ULSAN", "35.5384", "129.3114"},
  {"SUWON", "37.2636", "127.0286"},
  {"JEJU", "33.4996", "126.5312"},
};
constexpr char kPath[] = "/city-settings.json";
constexpr char kTemporaryPath[] = "/city-settings.tmp";
}

bool selectPresetCity(const char *id, CitySettings &out) {
  if (!id) return false;
  for (const auto &stored : kCities) {
    PresetCity preset;
    memcpy_P(&preset, &stored, sizeof(preset));
    if (strcmp(id, preset.name)) continue;
    strcpy(out.city, preset.name);
    strcpy(out.name, preset.name);
    strcpy(out.latitude, preset.latitude);
    strcpy(out.longitude, preset.longitude);
    return true;
  }
  return false;
}

bool saveCitySettings(const CitySettings &settings, bool filesystemReady) {
  if (!filesystemReady) return false;
  StaticJsonDocument<256> document;
  document["schemaVersion"] = 1;
  document["city"] = settings.city;
  document["name"] = settings.name;
  document["latitude"] = settings.latitude;
  document["longitude"] = settings.longitude;
  File file = LittleFS.open(kTemporaryPath, "w");
  if (!file) return false;
  const size_t expected = measureJson(document);
  const bool written = serializeJson(document, file) == expected;
  file.close();
  if (!written || !LittleFS.rename(kTemporaryPath, kPath)) {
    LittleFS.remove(kTemporaryPath);
    return false;
  }
  return true;
}

void loadCitySettings(CitySettings &settings, bool filesystemReady) {
  settings = CitySettings{};
  if (!filesystemReady || !LittleFS.exists(kPath)) return;
  File file = LittleFS.open(kPath, "r");
  StaticJsonDocument<512> document;
  const bool malformed = !file || file.size() > 512 ||
      deserializeJson(document, file) || !document.is<JsonObject>();
  file.close();
  const auto schema = inspectStoredConfigSchema(document.as<JsonObjectConst>(), 1);
  if (malformed || schema.state != StoredConfigSchemaState::Current) {
    Serial.println(F("City settings: malformed or unsupported schema; using SEOUL"));
    quarantineStoredConfigFile(kPath, "/city-settings.invalid");
    if (!saveCitySettings(settings, filesystemReady))
      Serial.println(F("City settings: could not persist defaults"));
    return;
  }
  const char *id = document["city"] | "";
  bool repaired = schema.legacy;
  if (!strcmp(id, "CUSTOM")) {
    strcpy(settings.city, "CUSTOM");
    const char *name = document["name"] | "";
    if (cityNameValid(name)) strcpy(settings.name, name);
    else repaired = true;
    if (!normalizeCoordinate(document["latitude"] | "", 90, settings.latitude)) repaired = true;
    if (!normalizeCoordinate(document["longitude"] | "", 180, settings.longitude)) repaired = true;
  } else if (!selectPresetCity(id, settings)) repaired = true;
  // Preset metadata is authoritative; normalize individual missing/invalid fields.
  repaired |= strcmp(document["name"] | "", settings.name) != 0 ||
      strcmp(document["latitude"] | "", settings.latitude) != 0 ||
      strcmp(document["longitude"] | "", settings.longitude) != 0;
  if (repaired) {
    Serial.println(F("City settings: repairing invalid fields"));
    if (!saveCitySettings(settings, filesystemReady))
      Serial.println(F("City settings: could not persist repairs"));
  }
}
