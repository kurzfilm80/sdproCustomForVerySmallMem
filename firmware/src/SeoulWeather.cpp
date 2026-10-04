#include "SeoulWeather.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <cmath>

SeoulWeatherError seoulWeatherLastError = SeoulWeatherError::None;
int seoulWeatherLastHttpCode = 0;
String seoulWeatherLastErrorText;
uint32_t seoulWeatherLastAttemptAt = 0;

static const char kWeatherUrl[] =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=37.5665&longitude=126.9780"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&timezone=Asia%2FSeoul&forecast_days=3";

const char* seoulWeatherErrorName(SeoulWeatherError e) {
  switch (e) {
    case SeoulWeatherError::None: return "OK";
    case SeoulWeatherError::Wifi: return "WIFI";
    case SeoulWeatherError::Begin: return "HTTP BEGIN";
    case SeoulWeatherError::Http: return "HTTP";
    case SeoulWeatherError::Json: return "JSON";
    case SeoulWeatherError::MissingDaily: return "DATA";
    case SeoulWeatherError::Dns: return "DNS";
  }
  return "UNKNOWN";
}

static bool fail(SeoulWeatherError e, const String& text, int httpCode = 0) {
  seoulWeatherLastError = e;
  seoulWeatherLastErrorText = text;
  seoulWeatherLastHttpCode = httpCode;
  Serial.printf("Weather error [%s] HTTP=%d: %s\n",
                seoulWeatherErrorName(e), httpCode, text.c_str());
  return false;
}

bool fetchSeoulWeather(SeoulWeatherDay out[3]) {
  seoulWeatherLastAttemptAt = millis();
  seoulWeatherLastHttpCode = 0;
  seoulWeatherLastErrorText = "";

  if (WiFi.status() != WL_CONNECTED)
    return fail(SeoulWeatherError::Wifi, "Wi-Fi not connected");

  Serial.printf("Weather heap=%u maxBlock=%u fragmentation=%u%%\n",
                ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),
                ESP.getHeapFragmentation());
  IPAddress address;
  if (!WiFi.hostByName("api.open-meteo.com", address, 12000))
    return fail(SeoulWeatherError::Dns, "Open-Meteo DNS lookup failed");
  Serial.printf("Open-Meteo IP: %s\n", address.toString().c_str());

  // Open-Meteo uses HTTPS. ESP8266 has a small heap; using setInsecure()
  // avoids storing a CA certificate while still encrypting the connection.
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12000);

  HTTPClient http;
  http.setTimeout(12000);
  http.setReuse(false);
  http.useHTTP10(true);  // simpler body handling on ESP8266

  if (!http.begin(client, kWeatherUrl))
    return fail(SeoulWeatherError::Begin, "http.begin failed");

  const int code = http.GET();
  seoulWeatherLastHttpCode = code;
  if (code != HTTP_CODE_OK) {
    String detail = code < 0 ? http.errorToString(code) : String("status ") + code;
    if (code < 0) {
      char sslText[128]{};
      const int sslError = client.getLastSSLError(sslText, sizeof(sslText));
      Serial.printf("Weather TLS error=%d: %s; heap=%u maxBlock=%u\n",
                    sslError, sslText, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
      if (sslError) detail = String("TLS ") + sslError + ": " + sslText;
    }
    http.end();
    return fail(SeoulWeatherError::Http, detail, code);
  }

  // Filter only the daily arrays we need to reduce ESP8266 heap usage.
  StaticJsonDocument<256> filter;
  filterWeather(filter);

  DynamicJsonDocument doc(2048);
  DeserializationError err =
      deserializeJson(doc, http.getStream(),
                      DeserializationOption::Filter(filter));
  http.end();

  if (err)
    return fail(SeoulWeatherError::Json, err.c_str(), code);

  if (!decodeWeather(doc["daily"].as<JsonObjectConst>(), out))
    return fail(SeoulWeatherError::MissingDaily, "invalid daily forecast/date", code);

  seoulWeatherLastError = SeoulWeatherError::None;
  seoulWeatherLastErrorText = "OK";
  return true;
}
