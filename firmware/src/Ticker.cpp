#ifdef SDPRO_TICKER
// Bounded SD PRO adaptation of giovi321/smalltv-mod ticker (WTFPL v2).
// Source revision and differences are documented in docs/ticker-firmware.md.
#include "Ticker.h"
#include "TickerJson.h"
#include "StoredConfigFile.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "WifiPower.h"
#include <LittleFS.h>
#include "TickerAssets.generated.h"
#include "fonts/InterTightBold18.h"
#include "fonts/InterTightBold24.h"
#include "fonts/InterTightBold36.h"
#include "fonts/InterTightDigits48.h"
#include "fonts/InterTightCompact13.h"
namespace {
constexpr char kPath[] = "/ticker-settings.json";
constexpr char kTemp[] = "/ticker-settings.tmp";
TickerSettings settings;
TickerQuote quotes[kTickerLimit];
bool fsReady = false, dirty = true;
uint8_t page = 0, pollIndex = 0;
uint32_t rotatedAt = 0, lastPoll = 0;

bool save(const TickerSettings &s) {
  if (!fsReady) return false;
  StaticJsonDocument<2048> doc;
  encodeTickerSettings(doc.to<JsonObject>(), s);
  if (doc.overflowed()) return false;
  File f = LittleFS.open(kTemp, "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) == measureJson(doc);
  f.close();
  if (!ok || !LittleFS.rename(kTemp, kPath)) { LittleFS.remove(kTemp); return false; }
  return true;
}
void resetRuntime() {
  for (auto &q : quotes) q = TickerQuote{};
  page = pollIndex = 0;
  rotatedAt = millis();
  dirty = true;
}
class BoundedQuoteStream : public Stream {
 public:
  explicit BoundedQuoteStream(Stream &source) : source_(source) { setTimeout(6000); }
  int available() override { return remaining_ ? source_.available() : 0; }
  int read() override {
    if (!remaining_) return -1;
    const int value = source_.read();
    if (value >= 0) --remaining_;
    return value;
  }
  int peek() override { return remaining_ ? source_.peek() : -1; }
  void flush() override {}
  size_t write(uint8_t) override { return 0; }
 private:
  Stream &source_;
  uint32_t remaining_ = 32768;
};
bool fetch(uint8_t index) {
  TickerQuote &q = quotes[index];
  // Full TLS receive buffer is retained: Yahoo MFLN support is not assumed.
  // Never start a handshake on a fragmented/low heap.
  if (ESP.getFreeHeap() < 30000 || ESP.getMaxFreeBlockSize() < 20000) { q.httpCode = -100; return false; }
  ActiveWifiTransfer activeTransfer;
  BearSSL::WiFiClientSecure client;
  client.setInsecure(); // Same public-price threat model as upstream; see docs.
  client.setTimeout(6000);
  HTTPClient http;
  http.setTimeout(6000);
  http.setReuse(false);
  http.useHTTP10(true);
  String symbol = settings.positions[index].symbol;
  symbol.replace("^", "%5E"); symbol.replace("=", "%3D");
  const char *host = q.failures % 2 ? "query2.finance.yahoo.com" : "query1.finance.yahoo.com";
  const bool dailyRequest = q.dailyPending;
  const auto *period = tickerGraphPeriod(dailyRequest ? "1d" : settings.graphRange);
  String url = String("https://") + host + "/v8/finance/chart/" + symbol +
      "?range=" + period->range + "&interval=" + period->interval;
  if (!http.begin(client, url)) { q.httpCode = -101; return false; }
  http.addHeader("User-Agent", "Mozilla/5.0 SDPRO-Ticker");
  q.httpCode = http.GET();
  bool ok = false;
  if (q.httpCode == HTTP_CODE_OK && http.getSize() <= 32768) {
    TickerQuote next = q;
    BoundedQuoteStream stream(http.getStream());
    if (parseTickerYahoo(stream, next, !strcmp(period->range, "1d"))) {
      if (dailyRequest) ok = tickerApplyDailyQuote(q, next);
      else {
        q = next;
        q.dailyPending = !q.hasChange && strcmp(period->range, "1d");
        ok = true;
      }
    }
  }
  http.end();
  return ok;
}
}
void tickerLoad(bool ready) {
  fsReady = ready;
  settings = TickerSettings{};
  if (!ready || !LittleFS.exists(kPath)) return;
  File f = LittleFS.open(kPath, "r");
  StaticJsonDocument<2048> doc;
  const bool malformed = !f || f.size() > 2048 || deserializeJson(doc, f);
  f.close();
  bool repaired = false;
  if (malformed || !decodeTickerSettings(doc.as<JsonObjectConst>(), settings, repaired)) {
    quarantineStoredConfigFile(kPath, "/ticker-settings.invalid");
    settings = TickerSettings{};
    repaired = true;
    Serial.println(F("Ticker settings quarantined; using defaults"));
  }
  if (repaired && !save(settings)) Serial.println(F("Ticker settings normalization failed"));
  resetRuntime();
}
void tickerRoutes(ESP8266WebServer &server, bool (*authorized)()) {
  server.on("/", HTTP_GET, [&server, authorized] {
    if (!authorized()) return;
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char *>(kTickerGzip), sizeof(kTickerGzip));
  });
  server.on("/api/v1/tickers", HTTP_GET, [&server, authorized] {
    if (!authorized()) return;
    DynamicJsonDocument doc(4096);
    if (!doc.capacity()) { server.send(503, "text/plain", "Low heap"); return; }
    encodeTickerSettings(doc.to<JsonObject>(), settings);
    doc["freeHeapBytes"] = ESP.getFreeHeap();
    doc["maximumFreeBlockBytes"] = ESP.getMaxFreeBlockSize();
    auto array = doc.createNestedArray("quotes");
    for (uint8_t i = 0; i < settings.count; ++i) {
      auto v = array.createNestedObject(); const auto &q = quotes[i];
      v["symbol"] = settings.positions[i].symbol;
      v["valid"] = q.valid; v["error"] = q.error; v["httpCode"] = q.httpCode;
      v["hasChange"] = q.hasChange; v["dailyPending"] = q.dailyPending;
      v["price"] = q.price; v["change"] = q.change; v["percent"] = q.percent;
      v["currency"] = q.currency; v["ageSeconds"] = q.valid ? (millis() - q.lastOk) / 1000 : 0;
    }
    String body; serializeJson(doc, body); server.send(200, "application/json", body);
  });
  server.on("/api/v1/tickers", HTTP_POST, [&server, authorized] {
    if (!authorized()) return;
    const String &body = server.arg("plain");
    if (body.length() > 2048) { server.send(413, "text/plain", "Settings too large"); return; }
    StaticJsonDocument<2048> doc;
    TickerSettings next; bool repaired = false;
    if (deserializeJson(doc, body) || !decodeTickerSettings(doc.as<JsonObjectConst>(), next, repaired) || repaired) {
      server.send(422, "text/plain", "Invalid schema, symbols, names, graph period or intervals"); return;
    }
    if (!save(next)) { server.send(503, "text/plain", "Settings could not be saved"); return; }
    settings = next; resetRuntime(); server.send(200, "application/json", "{\"saved\":true}");
  });
  server.on("/api/v1/tickers/refresh", HTTP_POST, [&server, authorized] {
    if (!authorized()) return;
    for (auto &q : quotes) { q.nextTry = millis(); q.failures = 0; q.dailyPending = false; }
    server.send(202, "application/json", "{\"queued\":true}");
  });
}
bool tickerAdvance(uint32_t now) {
  if (settings.count && now - rotatedAt >= settings.rotateSeconds * 1000UL) {
    page = (page + 1) % settings.count; rotatedAt = now; dirty = true;
  }
  return dirty;
}
bool tickerPoll(uint32_t now) {
  if (!settings.count || now - lastPoll < 1000) return false;
  // One symbol per loop; retain good cached data on every failed refresh.
  for (uint8_t n = 0; n < settings.count; ++n) {
    const uint8_t i = pollIndex; pollIndex = (pollIndex + 1) % settings.count;
    auto &q = quotes[i];
    if (!tickerDue(now, q.nextTry)) continue;
    lastPoll = now;
    const bool ok = fetch(i);
    q.error = !ok;
    if (ok) { q.valid = true; q.lastOk = millis(); q.failures = 0; }
    else if (q.failures < 10) ++q.failures;
    q.nextTry = millis() + (ok ? (q.dailyPending ? 1000UL : settings.refreshSeconds * 1000UL) : tickerRetryMs(q.failures));
    dirty |= i == page;
    return i == page;
  }
  return false;
}
void tickerDraw(TFT_eSPI &display) {
  dirty = false;
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.setFreeFont(&InterTightBold18);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  if (!settings.count) {
    display.drawString("Add tickers in web UI", 120, 80);
    display.drawString(WiFi.localIP().toString(), 120, 120);
    return;
  }
  const auto &p = settings.positions[page]; const auto &q = quotes[page];
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  const char *name = tickerDisplayName(p);
  const char *rangeLabel = tickerGraphPeriod(settings.graphRange)->label;
  display.setFreeFont(&InterTightBold24);
  const int16_t nameWidth = 2 * (236 - display.textWidth(rangeLabel) - 6 - 120);
  char label[sizeof(p.name)];
  strlcpy(label, name, sizeof(label));
  while (display.textWidth(label) > nameWidth && strlen(label) > 3) {
    const size_t length = strlen(label);
    memcpy(label + length - 4, "...", 4);
  }
  display.setTextDatum(C_BASELINE);
  display.drawString(label, 120, kTickerNameBaseline);
  display.setTextDatum(R_BASELINE);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString(rangeLabel, 236, kTickerNameBaseline);
  display.setTextDatum(C_BASELINE);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setFreeFont(&InterTightBold18);
  if (!q.valid) { display.drawString(q.error ? "Fetch failed / retrying" : "Loading...", 120, 110); return; }
  const float movement = q.hasChange ? q.change : 0;
  const uint16_t color = movement > 0 ? TFT_RED : movement < 0 ? TFT_BLUE : TFT_LIGHTGREY;
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  char number[24], change[24], percent[24], line[64];
  tickerFormatAmount(q.price, q.currency, number, sizeof(number));
  const char *prefix = tickerCurrencyPrefix(q.currency);
  // Separate numeric and currency fonts retain 48px prices without embedding
  // another full alphabet. Fit the combined width before drawing either part.
  const GFXfont *numberFont = &InterTightDigits48;
  const GFXfont *prefixFont = &InterTightBold36;
  int16_t numberWidth = 0, prefixWidth = 0;
  for (uint8_t step = 0; step < 4; ++step) {
    if (step == 1) numberFont = prefixFont = &InterTightBold36;
    if (step == 2) numberFont = prefixFont = &InterTightBold24;
    if (step == 3) numberFont = prefixFont = &InterTightBold18;
    display.setFreeFont(numberFont); numberWidth = display.textWidth(number);
    display.setFreeFont(prefixFont); prefixWidth = display.textWidth(prefix);
    if (numberWidth + prefixWidth + (prefixWidth ? 4 : 0) <= 232) break;
  }
  const int16_t left = 120 - (numberWidth + prefixWidth + (prefixWidth ? 4 : 0)) / 2;
  display.setTextDatum(L_BASELINE);
  display.drawString(prefix, left, kTickerPriceBaseline);
  if (!strcmp(q.currency, "KRW")) {
    // The bundled Latin font has no U+20A9 glyph. Reuse its W outline to
    // render a won sign without adding a font or allocating a bitmap.
    const int16_t upper = prefixFont == &InterTightBold36 ? 17 : prefixFont == &InterTightBold24 ? 11 : 8;
    const int16_t lower = prefixFont == &InterTightBold36 ? 11 : prefixFont == &InterTightBold24 ? 7 : 5;
    display.drawFastHLine(left, kTickerPriceBaseline - upper, prefixWidth, TFT_WHITE);
    display.drawFastHLine(left, kTickerPriceBaseline - lower, prefixWidth, TFT_WHITE);
  }
  display.setFreeFont(numberFont);
  display.drawString(number, left + prefixWidth + (prefixWidth ? 4 : 0), kTickerPriceBaseline);
  display.setTextDatum(C_BASELINE);
  display.setTextColor(color, TFT_BLACK);
  tickerFormatAmount(fabsf(q.change), q.currency, change, sizeof(change)); tickerFormat(fabsf(q.percent), percent, sizeof(percent));
  snprintf(line, sizeof(line), "%s (%s%%)", change, percent);
  const char *changeText = q.hasChange ? line : q.dailyPending
      ? "" : "Change unavailable";
  const int16_t iconWidth = q.hasChange && q.change != 0 ? 16 : 0;
  const int16_t changeWidth = 232 - iconWidth;
  display.setFreeFont(&InterTightBold24);
  if (display.textWidth(changeText) > changeWidth) display.setFreeFont(&InterTightBold18);
  if (display.textWidth(changeText) > changeWidth) display.setFreeFont(&InterTightCompact13);
  if (display.textWidth(changeText) > changeWidth && q.hasChange) {
    snprintf(line, sizeof(line), "%s%%", percent);
    changeText = line;
    display.setFreeFont(&InterTightBold24);
    if (display.textWidth(changeText) > changeWidth) display.setFreeFont(&InterTightBold18);
  }
  const int16_t changeLeft = 120 - (display.textWidth(changeText) + iconWidth) / 2;
  if (iconWidth) {
    const int16_t top = kTickerChangeBaseline - 13, bottom = kTickerChangeBaseline - 3;
    if (q.change > 0) display.fillTriangle(changeLeft + 6, top, changeLeft, bottom, changeLeft + 12, bottom, color);
    else display.fillTriangle(changeLeft, top, changeLeft + 12, top, changeLeft + 6, bottom, color);
  }
  display.setTextDatum(L_BASELINE);
  if (*changeText) display.drawString(changeText, changeLeft + iconWidth, kTickerChangeBaseline);
  display.setTextDatum(C_BASELINE);
  const bool profitVisible = p.quantity > 0 && p.cost > 0;
  if (profitVisible) {
    tickerFormat((q.price / p.cost - 1) * 100, percent, sizeof(percent));
    snprintf(line, sizeof(line), "P/L %s%%", percent);
    display.setTextColor(q.price > p.cost ? TFT_RED : q.price < p.cost ? TFT_BLUE : TFT_LIGHTGREY, TFT_BLACK);
    display.setFreeFont(&InterTightBold24);
    if (display.textWidth(line) > 232) display.setFreeFont(&InterTightBold18);
    if (display.textWidth(line) > 232) display.setFreeFont(&InterTightCompact13);
    display.drawString(line, 120, kTickerProfitBaseline);
  }
  if (q.sparkCount > 1) {
    const float graphMovement = tickerGraphMovement(q);
    const uint16_t graphColor = graphMovement > 0 ? TFT_RED : graphMovement < 0 ? TFT_BLUE : TFT_LIGHTGREY;
    float lo = q.spark[0], hi = lo;
    for (uint8_t i = 1; i < q.sparkCount; ++i) { lo = fminf(lo, q.spark[i]); hi = fmaxf(hi, q.spark[i]); }
    const int16_t top = tickerGraphTop(profitVisible);
    int px = kTickerGraphLeft, py = tickerGraphY(q.spark[0], lo, hi, top);
    for (uint8_t i = 1; i < q.sparkCount; ++i) {
      const int x = kTickerGraphLeft + (kTickerGraphRight - kTickerGraphLeft) * i / (q.sparkCount - 1);
      const int y = tickerGraphY(q.spark[i], lo, hi, top);
      display.drawLine(px, py, x, y, graphColor);
      display.drawLine(px, py + 1, x, y + 1, graphColor);
      px = x; py = y;
    }
  }
}
#endif
