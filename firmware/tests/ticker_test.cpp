#include <cassert>
#include <sstream>
#include "TickerJson.h"

int main() {
  for (bool profit : {false, true}) {
    const int16_t top = tickerGraphTop(profit);
    assert(tickerGraphY(10, 10, 20, top) == kTickerGraphBottom);
    assert(tickerGraphY(20, 10, 20, top) == top);
    assert(tickerGraphY(0, 10, 20, top) == kTickerGraphBottom);
    assert(tickerGraphY(30, 10, 20, top) == top);
    assert(tickerGraphY(10, 10, 10, top) == (top + kTickerGraphBottom) / 2);
    for (int i = 0; i <= 100; ++i) {
      const int16_t y = tickerGraphY(i / 100.0f, 0, 1, top);
      assert(y >= top && y + 1 <= 236);
    }
  }
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
  assert(!strcmp(reloaded.graphRange, "1d") && !reloaded.positions[0].name[0]);
  assert(!strcmp(tickerDisplayName(reloaded.positions[0]), "AAPL"));
  doc["graphRange"] = "6mo";
  doc["positions"][0]["name"] = "Apple";
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && !repaired);
  assert(!strcmp(tickerDisplayName(settings.positions[0]), "Apple"));
  assert(!strcmp(tickerGraphPeriod(settings.graphRange)->interval, "1wk"));
  saved.clear(); encodeTickerSettings(saved.to<JsonObject>(), settings);
  assert(!saved.overflowed());
  assert(decodeTickerSettings(saved.as<JsonObjectConst>(), reloaded, repaired) && !repaired);
  assert(!strcmp(reloaded.graphRange, "6mo") && !strcmp(reloaded.positions[0].name, "Apple"));
  doc["graphRange"] = "bad"; doc["positions"][0]["name"] = 123;
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired) && repaired);
  assert(!strcmp(settings.graphRange, "1d") && !settings.positions[0].name[0]);
  doc.remove("graphRange"); doc["positions"][0].remove("name");
  assert(tickerNameValid("SAMSUNG Electronics")); assert(tickerNameValid(""));
  assert(!tickerNameValid("012345678901234567890123")); assert(!tickerNameValid("bad\nname"));
  assert(!tickerNameValid("삼성전자"));
  for (const char *range : {"1d", "5d", "1mo", "3mo", "6mo", "1y"}) assert(tickerGraphPeriod(range));
  assert(!tickerGraphPeriod("max") && !tickerGraphPeriod(nullptr));
  assert(!strcmp(tickerGraphPeriod("1d")->label, "1D"));
  assert(!strcmp(tickerGraphPeriod("1mo")->label, "1M"));
  assert(!strcmp(tickerGraphPeriod("1y")->label, "1Y"));
  assert(!strcmp(tickerCurrencyPrefix("USD"), "$")); assert(!strcmp(tickerCurrencyPrefix("KRW"), "W"));
  assert(!strcmp(tickerCurrencyPrefix("JPY"), "JPY"));
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
  std::istringstream longRange(R"({"chart":{"result":[{"meta":{"regularMarketPrice":110,"chartPreviousClose":80},"indicators":{"quote":[{"close":[80,100,110]}]}}]}})");
  assert(parseTickerYahoo(longRange, quote, false)); assert(!quote.hasChange && quote.sparkCount == 3);
  std::istringstream dailyClose(R"({"chart":{"result":[{"meta":{"regularMarketPrice":110,"previousClose":100,"chartPreviousClose":80}}]}})");
  assert(parseTickerYahoo(dailyClose, quote, false)); assert(quote.hasChange && quote.change == 10);
  TickerQuote chart;
  chart.price = 110; chart.sparkCount = 3;
  chart.spark[0] = 80; chart.spark[1] = 100; chart.spark[2] = 110;
  chart.valid = true; chart.dailyPending = true;
  TickerQuote daily;
  assert(!tickerApplyDailyQuote(chart, daily) && chart.dailyPending && chart.sparkCount == 3);
  std::istringstream liveDaily(R"({"chart":{"result":[{"meta":{"regularMarketPrice":262000,"previousClose":268500,"chartPreviousClose":268500,"currency":"KRW"},"indicators":{"quote":[{"close":[268500,262000]}]}}]}})");
  assert(parseTickerYahoo(liveDaily, daily));
  assert(tickerApplyDailyQuote(chart, daily));
  assert(chart.hasChange && !chart.dailyPending && chart.change == -6500);
  assert(chart.sparkCount == 3 && chart.spark[0] == 80 && chart.spark[2] == 110);
  assert(!strcmp(chart.currency, "KRW") && chart.valid);
  // Daily direction and period direction can disagree in either direction.
  TickerQuote direction;
  direction.valid = true; direction.hasChange = true; direction.sparkCount = 2;
  direction.spark[0] = 200; direction.spark[1] = 90;
  direction.price = 100; direction.change = 10;
  assert(direction.change > 0 && tickerGraphMovement(direction) == -100);
  direction.spark[0] = 80; direction.change = -10;
  assert(direction.change < 0 && tickerGraphMovement(direction) == 20);
  direction.spark[0] = 100;
  assert(tickerGraphMovement(direction) == 0);
  direction.sparkCount = 1;
  assert(tickerGraphMovement(direction) == 0);
  direction.sparkCount = 0;
  assert(tickerGraphMovement(direction) == 0);
  tickerFormatAmount(262000, "KRW", formatted, sizeof(formatted)); assert(!strcmp(formatted, "262000"));
  tickerFormatAmount(123.5f, "KRW", formatted, sizeof(formatted)); assert(!strcmp(formatted, "124"));
  tickerFormatAmount(-123.5f, "KRW", formatted, sizeof(formatted)); assert(!strcmp(formatted, "-124"));
  tickerFormatAmount(-0.1f, "KRW", formatted, sizeof(formatted)); assert(!strcmp(formatted, "0"));
  tickerFormatAmount(NAN, "KRW", formatted, sizeof(formatted)); assert(!strcmp(formatted, "--"));
  tickerFormatAmount(1000000000, "KRW", formatted, sizeof(formatted)); assert(!strcmp(formatted, "1000000000"));
  tickerFormatAmount(123.5f, "USD", formatted, sizeof(formatted)); assert(!strcmp(formatted, "123.50"));
  TickerSettings full;
  full.count = kTickerLimit;
  for (unsigned i = 0; i < kTickerLimit; ++i) {
    snprintf(full.positions[i].symbol, sizeof(full.positions[i].symbol), "LONG-SYMBOL-NUMBER-%u", i);
    snprintf(full.positions[i].name, sizeof(full.positions[i].name), "LONG DISPLAY NAME NO. %u", i);
    full.positions[i].quantity = full.positions[i].cost = 1000000000;
  }
  saved.clear(); encodeTickerSettings(saved.to<JsonObject>(), full);
  assert(!saved.overflowed() && measureJson(saved) <= 2048);
  encoded.clear(); serializeJson(saved, encoded);
  doc.clear(); assert(!deserializeJson(doc, encoded));
  assert(decodeTickerSettings(doc.as<JsonObjectConst>(), reloaded, repaired) && !repaired && reloaded.count == kTickerLimit);
}
