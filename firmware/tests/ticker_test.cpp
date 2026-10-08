#include <cassert>
#include <sstream>
#include "TickerJson.h"

int main() {
  assert(tickerSymbolValid("BTC-USD")); assert(tickerSymbolValid("005930.KS"));
  assert(tickerSymbolValid("^GSPC")); assert(tickerSymbolValid("EURUSD=X"));
  for (const char *s : {"", "AAPL?token=x", "aapl", "A B", "012345678901234567890123"}) assert(!tickerSymbolValid(s));
  assert(tickerDue(5, UINT32_MAX - 5)); assert(!tickerDue(UINT32_MAX - 5, 5));
  assert(tickerRetryMs(1) == 15000); assert(tickerRetryMs(10) == 300000);
  char formatted[24]; tickerFormat(-123.45f, formatted, sizeof(formatted)); assert(!strcmp(formatted, "-123.45"));
  tickerFormat(NAN, formatted, sizeof(formatted)); assert(!strcmp(formatted, "--"));
  StaticJsonDocument<2048> doc;
  TickerSettings settings; bool repaired = false;
  const char *json = R"({"schemaVersion":1,"rotateSeconds":10,"refreshSeconds":300,"positions":[{"symbol":"AAPL","quantity":2,"cost":150}]})";
  assert(!deserializeJson(doc, json));
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && !repaired);
  assert(settings.count == 1 && settings.positions[0].quantity == 2);
  StaticJsonDocument<2048> saved; encodeTickerSettings(saved.to<JsonObject>(), settings);
  std::string encoded; serializeJson(saved, encoded); assert(!deserializeJson(doc, encoded));
  TickerSettings reloaded; assert(decodeTickerSettings(doc.as<JsonObjectConst>(), reloaded, repaired) && !repaired);
  assert(reloaded.positions[0].cost == 150);
  doc["schemaVersion"] = 2; assert(!decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired));
  doc["schemaVersion"] = "1"; assert(!decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired));
  doc["schemaVersion"] = 1; doc["rotateSeconds"] = 0;
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && repaired && settings.rotateSeconds == 10);
  doc["positions"][0]["quantity"] = -1;
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && repaired && settings.positions[0].quantity == 0);
  doc["positions"][0]["symbol"] = "<script>";
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && repaired && settings.count == 0);
  assert(!deserializeJson(doc, json)); auto positions = doc["positions"].as<JsonArray>();
  auto duplicate = positions.createNestedObject(); duplicate["symbol"] = "AAPL"; duplicate["quantity"] = 0; duplicate["cost"] = 0;
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && repaired && settings.count == 1);
  positions.clear();
  for (int i=0;i<9;++i) { auto p=positions.createNestedObject(); p["symbol"]=std::string("TEST")+std::to_string(i); p["quantity"]=0; p["cost"]=0; }
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && repaired && settings.count == 8);
  TickerQuote quote;
  std::istringstream payload(R"({"chart":{"result":[{"meta":{"regularMarketPrice":110,"chartPreviousClose":100,"currency":"USD"},"indicators":{"quote":[{"close":[100,null,105,110]}]}}]}})");
  assert(parseTickerYahoo(payload, quote)); assert(quote.price == 110 && quote.change == 10 && quote.percent == 10 && quote.sparkCount == 3);
  std::istringstream missing(R"({"chart":{"result":null,"error":{"code":"Not Found"}}})");
  assert(!parseTickerYahoo(missing, quote));
  std::string many = R"({"chart":{"result":[{"meta":{"regularMarketPrice":200},"indicators":{"quote":[{"close":[)";
  for (int i=0;i<100;++i) many += (i ? "," : "") + std::to_string(i);
  many += "]}]}}]}}"; std::istringstream stream(many);
  assert(parseTickerYahoo(stream, quote)); assert(quote.sparkCount == 32 && quote.spark[0] == 0 && quote.spark[31] == 99);
}
