#include <cassert>
#include <iostream>
#include <string>
#include "DisplaySettings.h"

int main() {
  DisplaySettings settings;
  assert(settings.autoRotate && settings.weatherPageSeconds == 15 && settings.airPageSeconds == 10);
  assert(settings.brightness == 80 && !settings.nightModeEnabled && settings.nightBrightness == 30);
  StaticJsonDocument<1024> document;
  assert(!deserializeJson(document, "{\"brightness\":75,\"unknown\":123}"));
  bool repaired = false;
  assert(decodeDisplaySettings(document.as<JsonObjectConst>(), settings, repaired));
  assert(repaired && settings.brightness == 75 && settings.weatherPageSeconds == 15);
  settings.autoRotate = false;
  settings.fixedPage = ForecastPage::AirQuality;
  settings.weatherPageSeconds = 60;
  settings.airPageSeconds = 5;
  settings.nightModeEnabled = true;
  encodeDisplaySettings(document.to<JsonObject>(), settings);
  assert(!document.overflowed());
  std::string saved;
  assert(serializeJson(document, saved) == measureJson(document));
  document.clear();
  assert(!deserializeJson(document, saved));
  DisplaySettings restored;
  assert(decodeDisplaySettings(document.as<JsonObjectConst>(), restored, repaired));
  assert(!repaired && !restored.autoRotate && restored.fixedPage == ForecastPage::AirQuality);
  assert(restored.weatherPageSeconds == 60 && restored.airPageSeconds == 5 && restored.brightness == 75);
  assert(restored.nightModeEnabled && restored.nightStart == 1320 && restored.nightEnd == 420);
  assert(document["nightStart"] == "22:00" && document["nightEnd"] == "07:00");
  document["weatherPageSeconds"] = 12;
  document["airPageSeconds"] = "15";
  document["nightStart"] = "24:00";
  document["nightBrightness"] = 35;
  document["brightness"] = 101;
  document["autoRotate"] = 1;
  assert(decodeDisplaySettings(document.as<JsonObjectConst>(), restored, repaired));
  assert(repaired && restored.weatherPageSeconds == 15 && restored.airPageSeconds == 10);
  assert(restored.brightness == 80 && restored.autoRotate && restored.nightBrightness == 30);
  assert(restored.fixedPage == ForecastPage::AirQuality && restored.nightEnd == 420);
  for (int schema : {0, 2, -1}) {
    document["schemaVersion"] = schema;
    assert(!decodeDisplaySettings(document.as<JsonObjectConst>(), restored, repaired));
  }
  uint16_t minutes = 999;
  for (const char *invalid : {"", "7:00", "24:00", "23:60", "ab:cd", "07:00:00"})
    assert(!parseDisplayTime(invalid, minutes));
  assert(parseDisplayTime("23:59", minutes) && minutes == 1439);
  assert(effectiveDisplayBrightness(settings, 1319, true) == 75);
  for (unsigned minute : {1320U, 1439U, 0U, 419U})
    assert(effectiveDisplayBrightness(settings, minute, true) == 30);
  assert(effectiveDisplayBrightness(settings, 420, true) == 75);
  assert(effectiveDisplayBrightness(settings, 0, false) == 75);
  settings.nightStart = 420; settings.nightEnd = 1320;
  assert(effectiveDisplayBrightness(settings, 419, true) == 75);
  assert(effectiveDisplayBrightness(settings, 420, true) == 30);
  assert(effectiveDisplayBrightness(settings, 1320, true) == 75);
  settings.nightEnd = settings.nightStart;
  assert(effectiveDisplayBrightness(settings, 420, true) == 75);
  PageCycle cycle;
  cycle.begin(100);
  assert(!cycle.advance(15099) && cycle.advance(15100));
  assert(!cycle.advance(25099) && cycle.advance(25100));
  assert(cycle.configure(30000, false, 5, 60, ForecastPage::AirQuality));
  assert(!cycle.advance(1000000) && cycle.page == ForecastPage::AirQuality);
  cycle.begin(100); assert(cycle.page == ForecastPage::AirQuality);
  assert(!cycle.configure(UINT32_MAX - 1000, true, 5, 60, ForecastPage::Weather));
  assert(!cycle.advance(58998) && cycle.advance(58999));
  assert(cycle.page == ForecastPage::Weather);
  assert(!cycle.advance(63998) && cycle.advance(63999));
  std::cout << "PASS: display JSON roundtrip, legacy schema, independent repair, night boundaries and page timers\n";
}
