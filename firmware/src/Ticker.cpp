#ifdef SDPRO_TICKER
// Bounded SD PRO adaptation of giovi321/smalltv-mod ticker (WTFPL v2).
// Source revision and differences are documented in docs/ticker-firmware.md.
#include "Ticker.h"
#include "TickerJson.h"
#include "StoredConfigFile.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LittleFS.h>
#include "TickerAssets.generated.h"
#include "fonts/InterTightBold18.h"
#include "fonts/InterTightBold24.h"
#include "fonts/InterTightBold36.h"
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
  String url = String("https://") + host + "/v8/finance/chart/" + symbol + "?range=1d&interval=30m";
  if (!http.begin(client, url)) { q.httpCode = -101; return false; }
  http.addHeader("User-Agent", "Mozilla/5.0 SDPRO-Ticker");
  q.httpCode = http.GET();
  bool ok = false;
  if (q.httpCode == HTTP_CODE_OK && http.getSize() <= 32768) {
    TickerQuote next = q;
    BoundedQuoteStream stream(http.getStream());
    if (parseTickerYahoo(stream, next)) { q = next; ok = true; }
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
      server.send(422, "text/plain", "Invalid schema, symbols, positions or intervals"); return;
    }
    if (!save(next)) { server.send(503, "text/plain", "Settings could not be saved"); return; }
    settings = next; resetRuntime(); server.send(200, "application/json", "{\"saved\":true}");
  });
  server.on("/api/v1/tickers/refresh", HTTP_POST, [&server, authorized] {
    if (!authorized()) return;
    for (auto &q : quotes) { q.nextTry = millis(); q.failures = 0; }
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
    q.nextTry = millis() + (ok ? settings.refreshSeconds * 1000UL : tickerRetryMs(q.failures));
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
  if (display.textWidth(p.symbol) > 232) display.setFreeFont(&InterTightCompact13);
  display.drawString(p.symbol, 120, 28);
  display.setFreeFont(&InterTightBold18);
  if (!q.valid) { display.drawString(q.error ? "Fetch failed / retrying" : "Loading...", 120, 110); return; }
  const uint16_t color = q.change >= 0 ? TFT_GREEN : TFT_RED;
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  char price[24], change[24], percent[24], line[64];
  tickerFormat(q.price, price, sizeof(price));
  display.setFreeFont(&InterTightBold36);
  if (display.textWidth(price) > 232) display.setFreeFont(&InterTightBold24);
  if (display.textWidth(price) > 232) display.setFreeFont(&InterTightBold18);
  display.drawString(price, 120, 75);
  display.setFreeFont(&InterTightBold18);
  display.drawString(q.currency, 120, 105);
  display.setTextColor(color, TFT_BLACK);
  tickerFormat(q.change, change, sizeof(change)); tickerFormat(q.percent, percent, sizeof(percent));
  snprintf(line, sizeof(line), "%s (%s%%)", change, percent);
  display.drawString(q.hasChange ? line : "Change unavailable", 120, 134);
  if (p.quantity > 0 && p.cost > 0) {
    tickerFormat((q.price / p.cost - 1) * 100, percent, sizeof(percent));
    snprintf(line, sizeof(line), "P/L %s%%", percent);
    display.setTextColor(q.price >= p.cost ? TFT_GREEN : TFT_RED, TFT_BLACK);
    display.drawString(line, 120, 158);
  }
  if (q.sparkCount > 1) {
    float lo = q.spark[0], hi = lo;
    for (uint8_t i = 1; i < q.sparkCount; ++i) { lo = fminf(lo, q.spark[i]); hi = fmaxf(hi, q.spark[i]); }
    const float span = hi > lo ? hi - lo : 1;
    int px = 6, py = 214 - int((q.spark[0] - lo) / span * 38);
    for (uint8_t i = 1; i < q.sparkCount; ++i) {
      const int x = 6 + 228 * i / (q.sparkCount - 1), y = 214 - int((q.spark[i] - lo) / span * 38);
      display.drawLine(px, py, x, y, color); px = x; py = y;
    }
  }
  display.setTextColor(q.error ? TFT_ORANGE : TFT_LIGHTGREY, TFT_BLACK);
  snprintf(line, sizeof(line), "%u/%u  %s", page + 1, settings.count, q.error ? "STALE / retrying" : "Yahoo / 1D");
  display.drawString(line, 120, 232);
}
#endif
