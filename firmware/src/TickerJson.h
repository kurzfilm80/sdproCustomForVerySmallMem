#pragma once
#include "TickerModel.h"
#include <ArduinoJson.h>
#include <initializer_list>

inline void encodeTickerSettings(JsonObject root, const TickerSettings &s) {
  root["schemaVersion"] = 1;
  root["rotateSeconds"] = s.rotateSeconds;
  root["refreshSeconds"] = s.refreshSeconds;
  JsonArray positions = root.createNestedArray("positions");
  for (uint8_t i = 0; i < s.count; ++i) {
    JsonObject p = positions.createNestedObject();
    p["symbol"] = s.positions[i].symbol;
    p["quantity"] = s.positions[i].quantity;
    p["cost"] = s.positions[i].cost;
  }
}
inline bool decodeTickerSettings(JsonObjectConst root, TickerSettings &s, bool &repaired) {
  if (root.isNull()) return false;
  if (root.containsKey("schemaVersion") &&
      (!root["schemaVersion"].is<unsigned>() || root["schemaVersion"].as<unsigned>() != 1)) return false;
  repaired = !root.containsKey("schemaVersion");
  s = TickerSettings{};
  const unsigned rotation = root["rotateSeconds"] | 10U;
  const unsigned refresh = root["refreshSeconds"] | 300U;
  if (root["rotateSeconds"].is<unsigned>() && rotation >= 3 && rotation <= 120) s.rotateSeconds = rotation;
  else repaired = true;
  if (root["refreshSeconds"].is<unsigned>() && refresh >= 60 && refresh <= 3600) s.refreshSeconds = refresh;
  else repaired = true;
  if (!root["positions"].is<JsonArrayConst>()) { repaired = true; return true; }
  for (JsonVariantConst v : root["positions"].as<JsonArrayConst>()) {
    const char *symbol = v["symbol"] | "";
    bool duplicate = false;
    for (uint8_t i = 0; i < s.count; ++i) duplicate |= !strcmp(symbol, s.positions[i].symbol);
    if (!v.is<JsonObjectConst>() || !tickerSymbolValid(symbol) || duplicate || s.count == kTickerLimit) {
      repaired = true; continue;
    }
    TickerPosition &p = s.positions[s.count++];
    std::memcpy(p.symbol, symbol, std::strlen(symbol) + 1);
    for (const char *field : {"quantity", "cost"}) {
      const float n = v[field] | 0.0f;
      if (!v[field].is<float>() || !tickerNumberValid(n)) { repaired = true; continue; }
      if (!strcmp(field, "quantity")) p.quantity = n; else p.cost = n;
    }
  }
  return true;
}
// Adapted from upstream parseYahoo: filter fields, retain finite closes,
// downsample to a fixed array, and derive daily change from previous close.
template <typename Input>
inline bool parseTickerYahoo(Input &stream, TickerQuote &q) {
  StaticJsonDocument<512> filter;
  auto meta = filter["chart"]["result"][0]["meta"].to<JsonObject>();
  meta["regularMarketPrice"] = true;
  meta["chartPreviousClose"] = true;
  meta["previousClose"] = true;
  meta["currency"] = true;
  filter["chart"]["result"][0]["indicators"]["quote"][0]["close"] = true;
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, stream, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12))) return false;
  JsonObjectConst result = doc["chart"]["result"][0];
  JsonObjectConst m = result["meta"];
  if (!m["regularMarketPrice"].is<float>()) return false;
  const float price = m["regularMarketPrice"];
  if (!tickerNumberValid(price)) return false;
  q.price = price;
  const char *currency = m["currency"] | "";
  std::snprintf(q.currency, sizeof(q.currency), "%s", currency);
  const float previous = m["chartPreviousClose"] | (m["previousClose"] | 0.0f);
  q.hasChange = tickerNumberValid(previous) && previous > 0;
  q.change = q.hasChange ? price - previous : 0;
  q.percent = q.hasChange ? q.change / previous * 100 : 0;
  q.sparkCount = 0;
  JsonArrayConst closes = result["indicators"]["quote"][0]["close"];
  uint16_t valid = 0;
  for (JsonVariantConst v : closes) if (v.is<float>() && tickerNumberValid(v.as<float>())) ++valid;
  const uint16_t want = valid < kSparkLimit ? valid : kSparkLimit;
  uint16_t i = 0, k = 0;
  for (JsonVariantConst v : closes) {
    if (!v.is<float>() || !tickerNumberValid(v.as<float>())) continue;
    const uint16_t target = want > 1 ? (uint32_t(k) * (valid - 1) + (want - 1) / 2) / (want - 1) : 0;
    if (i == target && k < want) { q.spark[q.sparkCount++] = v.as<float>(); ++k; }
    ++i;
  }
  return true;
}
