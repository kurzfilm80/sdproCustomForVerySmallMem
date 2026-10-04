#include "SeoulAirQuality.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

namespace {
constexpr char kAirQualityUrl[] =
    "https://air-quality-api.open-meteo.com/v1/air-quality"
    "?latitude=37.5665&longitude=126.9780"
    "&hourly=pm2_5,pm10&timezone=Asia%2FSeoul&forecast_days=3";
}

bool fetchSeoulAirQuality(SeoulAirQuality &out) {
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Air quality heap=%u maxBlock=%u\n",
                ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
  // Local lifetime releases all TLS buffers before the next API request.
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12000);
  HTTPClient http;
  http.setTimeout(12000);
  http.setReuse(false);
  http.useHTTP10(true);
  if (!http.begin(client, kAirQualityUrl)) {
    Serial.println(F("Air quality: HTTP begin failed"));
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char sslText[96]{};
    const int sslError = client.getLastSSLError(sslText, sizeof(sslText));
    Serial.printf("Air quality: HTTP=%d TLS=%d %s\n", code, sslError, sslText);
    http.end();
    return false;
  }

  StaticJsonDocument<256> filter;
  filterAirQuality(filter);
  DynamicJsonDocument document(kAirQualityJsonCapacity);
  const auto error = deserializeJson(document, http.getStream(),
                                    DeserializationOption::Filter(filter));
  http.end();
  if (error) {
    Serial.printf("Air quality JSON: %s\n", error.c_str());
    return false;
  }
  if (!decodeAirQuality(document["hourly"].as<JsonObjectConst>(), out)) {
    Serial.println(F("Air quality: missing or invalid hourly arrays/dates"));
    return false;
  }
  for (const auto &day : out.days)
    Serial.printf("Air quality MAX %s: PM2.5=%d valid=%d PM10=%d valid=%d\n",
                  day.date, day.pm25, day.pm25Valid, day.pm10, day.pm10Valid);
  return true;
}
