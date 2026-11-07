#include "SeoulAirQuality.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include "WifiPower.h"

int airQualityLastHttpCode = 0;
const char *airQualityLastError = "Not requested";

namespace {
constexpr char kAirQualityUrl[] =
    "https://air-quality-api.open-meteo.com/v1/air-quality"
    "?latitude=%s&longitude=%s"
    "&hourly=pm2_5,pm10&timezone=Asia%%2FSeoul&forecast_days=3";
}

bool fetchSeoulAirQuality(SeoulAirQuality &out, const CitySettings &city) {
  airQualityLastHttpCode = 0;
  airQualityLastError = "Wi-Fi not connected";
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Air quality heap=%u maxBlock=%u\n",
                ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
  // Local lifetime releases all TLS buffers before the next API request.
  ActiveWifiTransfer activeTransfer;
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12000);
  HTTPClient http;
  http.setTimeout(12000);
  http.setReuse(false);
  http.useHTTP10(true);
  airQualityLastError = "HTTP begin failed";
  char url[240];
  snprintf(url, sizeof(url), kAirQualityUrl, city.latitude, city.longitude);
  if (!http.begin(client, url)) {
    Serial.println(F("Air quality: HTTP begin failed"));
    return false;
  }
  const int code = http.GET();
  airQualityLastHttpCode = code;
  airQualityLastError = "HTTP/TLS request failed";
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
  airQualityLastError = "Invalid JSON";
  if (error) {
    Serial.printf("Air quality JSON: %s\n", error.c_str());
    return false;
  }
  airQualityLastError = "Invalid hourly data";
  if (!decodeAirQuality(document["hourly"].as<JsonObjectConst>(), out)) {
    Serial.println(F("Air quality: missing or invalid hourly arrays/dates"));
    return false;
  }
  airQualityLastError = "OK";
  for (const auto &day : out.days)
    Serial.printf("Air quality MAX %s: PM2.5=%d valid=%d PM10=%d valid=%d\n",
                  day.date, day.pm25, day.pm25Valid, day.pm10, day.pm10Valid);
  return true;
}
