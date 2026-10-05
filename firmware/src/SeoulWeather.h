#pragma once
#include <Arduino.h>

#include "SeoulWeatherData.h"
#include "CitySettings.h"

enum class SeoulWeatherError {
  None, Wifi, Begin, Http, Json, MissingDaily, Dns
};

extern SeoulWeatherError seoulWeatherLastError;
extern int seoulWeatherLastHttpCode;
extern String seoulWeatherLastErrorText;
extern uint32_t seoulWeatherLastAttemptAt;

bool fetchSeoulWeather(SeoulWeatherDay out[3], const CitySettings &city);
const char* seoulWeatherErrorName(SeoulWeatherError error);
