#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include "ForecastDate.h"

struct SeoulWeatherDay {
  float high = 0;
  float low = 0;
  int rainProbability = 0;
  int weatherCode = 0;
  char date[11]{};
};

inline void filterWeather(JsonDocument &filter) {
  filter["daily"]["time"] = true;
  filter["daily"]["weather_code"] = true;
  filter["daily"]["temperature_2m_max"] = true;
  filter["daily"]["temperature_2m_min"] = true;
  filter["daily"]["precipitation_probability_max"] = true;
}

inline bool decodeWeather(JsonObjectConst daily, SeoulWeatherDay out[3]) {
  JsonArrayConst dates = daily["time"];
  JsonArrayConst wc = daily["weather_code"];
  JsonArrayConst hi = daily["temperature_2m_max"];
  JsonArrayConst lo = daily["temperature_2m_min"];
  JsonArrayConst rain = daily["precipitation_probability_max"];
  if (dates.size()!=3 || wc.size()!=3 || hi.size()!=3 || lo.size()!=3 || rain.size()!=3)
    return false;
  ForecastDate expected;
  if (!parseForecastDate(dates[0] | "", expected)) return false;
  for (int i=0; i<3; ++i) {
    char date[11]; formatForecastDate(date, expected);
    if (!dates[i].is<const char *>() || strcmp(dates[i].as<const char *>(),date) ||
        !wc[i].is<int>() || !hi[i].is<float>() || !lo[i].is<float>() ||
        !rain[i].is<int>() || !std::isfinite(hi[i].as<float>()) ||
        !std::isfinite(lo[i].as<float>()) || rain[i].as<int>()<0 || rain[i].as<int>()>100)
      return false;
    expected = nextForecastDate(expected);
  }
  for (int i=0; i<3; ++i) {
    out[i].high=hi[i].as<float>(); out[i].low=lo[i].as<float>();
    out[i].weatherCode=wc[i].as<int>(); out[i].rainProbability=rain[i].as<int>();
    memcpy(out[i].date,dates[i].as<const char *>(),11);
  }
  return true;
}
