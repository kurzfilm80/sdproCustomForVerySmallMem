#pragma once
#include <Arduino.h>

#include "SeoulWeatherData.h"

enum class SeoulWeatherError {
  None, Wifi, Begin, Http, Json, MissingDaily, Dns
};

extern SeoulWeatherError seoulWeatherLastError;
extern int seoulWeatherLastHttpCode;
extern String seoulWeatherLastErrorText;
extern uint32_t seoulWeatherLastAttemptAt;

bool fetchSeoulWeather(SeoulWeatherDay out[3]);
const char* seoulWeatherErrorName(SeoulWeatherError error);
