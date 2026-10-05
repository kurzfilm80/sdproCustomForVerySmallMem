#pragma once

#include <ArduinoJson.h>
#include <cmath>
#include <climits>
#include "ForecastDate.h"
#include "CitySettings.h"

struct SeoulAirQualityDay {
  char date[11]{};
  int pm25 = 0;
  int pm10 = 0;
  bool pm25Valid = false;
  bool pm10Valid = false;
};
struct SeoulAirQuality { SeoulAirQualityDay days[3]{}; };

// 72 timestamps and two numeric arrays; other JSON fields are discarded.
constexpr size_t kAirQualityJsonCapacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) +
    3 * JSON_ARRAY_SIZE(72) + 72 * 17 + 64;

enum class DustGrade { Good, Normal, Bad, VeryBad };

inline DustGrade dustGrade(int value, bool pm25) {
  if (value <= (pm25 ? 15 : 30)) return DustGrade::Good;
  if (value <= (pm25 ? 35 : 80)) return DustGrade::Normal;
  if (value <= (pm25 ? 75 : 150)) return DustGrade::Bad;
  return DustGrade::VeryBad;
}

inline const char *dustGradeName(DustGrade grade) {
  switch (grade) {
    case DustGrade::Good: return "GOOD";
    case DustGrade::Normal: return "NORMAL";
    case DustGrade::Bad: return "BAD";
    case DustGrade::VeryBad: return "VERY BAD";
  }
  return "--";
}

inline void filterAirQuality(JsonDocument &filter) {
  filter["hourly"]["time"] = true;
  filter["hourly"]["pm2_5"] = true;
  filter["hourly"]["pm10"] = true;
}

inline bool validDustValue(JsonVariantConst value) {
  if (!value.is<float>()) return false;
  const float number = value.as<float>();
  return std::isfinite(number) && number >= 0 && number < static_cast<float>(INT_MAX);
}

inline bool decodeAirQuality(JsonObjectConst hourly, SeoulAirQuality &out) {
  JsonArrayConst times = hourly["time"];
  JsonArrayConst pm25 = hourly["pm2_5"];
  JsonArrayConst pm10 = hourly["pm10"];
  if (!times.size() || times.size()>72 || times.size()!=pm25.size() ||
      times.size()!=pm10.size()) return false;
  ForecastDate date;
  if (!parseForecastDate(times[0] | "", date)) return false;
  SeoulAirQuality result{};
  float max25[3]{}, max10[3]{};
  for (int i=0; i<3; ++i) {
    formatForecastDate(result.days[i].date,date);
    date = nextForecastDate(date);
  }
  for (size_t i=0; i<times.size(); ++i) {
    const char *timestamp = times[i] | "";
    ForecastDate parsed;
    if (strlen(timestamp)!=16 || !parseForecastDate(timestamp,parsed) ||
        timestamp[10]!='T' || timestamp[13]!=':' ||
        timestamp[11]<'0' || timestamp[11]>'2' ||
        timestamp[12]<'0' || timestamp[12]>'9' ||
        (timestamp[11]=='2' && timestamp[12]>'3') ||
        timestamp[14]<'0' || timestamp[14]>'5' ||
        timestamp[15]<'0' || timestamp[15]>'9') return false;
    for (int day=0; day<3; ++day) {
      if (strncmp(timestamp,result.days[day].date,10)) continue;
      if (validDustValue(pm25[i])) {
        const float value=pm25[i].as<float>();
        if (!result.days[day].pm25Valid || value>max25[day]) max25[day]=value;
        result.days[day].pm25Valid=true;
      }
      if (validDustValue(pm10[i])) {
        const float value=pm10[i].as<float>();
        if (!result.days[day].pm10Valid || value>max10[day]) max10[day]=value;
        result.days[day].pm10Valid=true;
      }
    }
  }
  for (int day=0; day<3; ++day) {
    result.days[day].pm25=static_cast<int>(roundf(max25[day]));
    result.days[day].pm10=static_cast<int>(roundf(max10[day]));
  }
  out=result;
  return true;
}

extern int airQualityLastHttpCode;
extern const char *airQualityLastError;
bool fetchSeoulAirQuality(SeoulAirQuality &out, const CitySettings &city);
