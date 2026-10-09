#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

constexpr uint8_t kTickerLimit = 8;
constexpr uint8_t kSparkLimit = 32;
constexpr int16_t kTickerNameBaseline = 26;
constexpr int16_t kTickerPriceBaseline = 78;
constexpr int16_t kTickerChangeBaseline = 106;
constexpr int16_t kTickerProfitBaseline = 134;
constexpr int16_t kTickerGraphLeft = 4;
constexpr int16_t kTickerGraphRight = 234;
constexpr int16_t kTickerGraphBottom = 235;
inline int16_t tickerGraphTop(bool profitVisible) { return profitVisible ? 146 : 118; }
inline int16_t tickerGraphY(float value, float low, float high, int16_t top) {
  if (high <= low) return (top + kTickerGraphBottom) / 2;
  const float fraction = fmaxf(0, fminf(1, (value - low) / (high - low)));
  return kTickerGraphBottom - static_cast<int16_t>(fraction * (kTickerGraphBottom - top) + 0.5f);
}
struct TickerPosition {
  char symbol[24]{};
  char name[24]{};
  float quantity = 0, cost = 0;
};
struct TickerSettings {
  TickerPosition positions[kTickerLimit]{};
  uint8_t count = 0;
  uint16_t rotateSeconds = 10;
  uint16_t refreshSeconds = 300;
  char graphRange[4] = "1d";
};
struct TickerGraphPeriod { const char *range; const char *interval; const char *label; };
inline const TickerGraphPeriod *tickerGraphPeriod(const char *range) {
  static constexpr TickerGraphPeriod periods[] = {
    {"1d", "30m", "1D"}, {"5d", "60m", "5D"}, {"1mo", "1d", "1M"},
    {"3mo", "1d", "3M"}, {"6mo", "1wk", "6M"}, {"1y", "1wk", "1Y"}
  };
  if (range) for (const auto &period : periods) if (!std::strcmp(period.range, range)) return &period;
  return nullptr;
}
inline bool tickerNameValid(const char *name) {
  if (!name || std::strlen(name) >= sizeof(TickerPosition::name)) return false;
  for (; *name; ++name) if (*name < 32 || *name > 126) return false;
  return true;
}
inline const char *tickerDisplayName(const TickerPosition &position) {
  return position.name[0] ? position.name : position.symbol;
}
// KRW's W is overlaid with two strokes by the renderer to form the won sign.
inline const char *tickerCurrencyPrefix(const char *currency) {
  if (!std::strcmp(currency, "USD")) return "$";
  if (!std::strcmp(currency, "KRW")) return "W";
  return currency;
}
struct TickerQuote {
  float price = 0, change = 0, percent = 0;
  float spark[kSparkLimit]{};
  char currency[8]{};
  uint8_t sparkCount = 0, failures = 0;
  bool valid = false, error = false, hasChange = false, dailyPending = false;
  uint32_t nextTry = 0, lastOk = 0;
  int httpCode = 0;
};
// Daily quotes may update the current price without replacing the period chart.
inline float tickerGraphMovement(const TickerQuote &quote) {
  return quote.valid && quote.sparkCount > 1 ? quote.price - quote.spark[0] : 0;
}
inline bool tickerApplyDailyQuote(TickerQuote &quote, const TickerQuote &daily) {
  if (!daily.hasChange) return false;
  quote.price = daily.price;
  quote.change = daily.change;
  quote.percent = daily.percent;
  quote.hasChange = true;
  quote.dailyPending = false;
  if (daily.currency[0]) std::memcpy(quote.currency, daily.currency, sizeof(quote.currency));
  return true;
}
inline bool tickerSymbolValid(const char *s) {
  if (!s || !*s || std::strlen(s) >= sizeof(TickerPosition::symbol)) return false;
  for (; *s; ++s) {
    if (!((*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
          *s == '.' || *s == '-' || *s == '^' || *s == '=' || *s == '_')) return false;
  }
  return true;
}
inline bool tickerNumberValid(float v) { return std::isfinite(v) && v >= 0 && v <= 1000000000.0f; }
inline bool tickerDue(uint32_t now, uint32_t due) { return static_cast<int32_t>(now - due) >= 0; }
inline uint32_t tickerRetryMs(uint8_t failures) {
  return failures < 5 ? (15000UL << (failures ? failures - 1 : 0)) : 300000UL;
}
// Integer formatting avoids pulling libc floating printf into the OTA image.
inline void tickerFormat(float value, char *out, size_t size) {
  if (!std::isfinite(value) || std::fabs(value) > 1000000000.0f) {
    std::snprintf(out, size, "--"); return;
  }
  const bool negative = value < 0;
  const float magnitude = std::fabs(value);
  const unsigned digits = magnitude > 0 && magnitude < 0.01f ? 6 : magnitude > 0 && magnitude < 1 ? 4 : 2;
  const unsigned scale = digits == 6 ? 1000000 : digits == 4 ? 10000 : 100;
  const uint64_t units = static_cast<uint64_t>(double(magnitude) * scale + 0.5);
  std::snprintf(out, size, "%s%lu.%0*u", negative ? "-" : "",
      static_cast<unsigned long>(units / scale), digits, static_cast<unsigned>(units % scale));
}

// Round won amounts to whole units without enabling libc floating printf.
inline void tickerFormatAmount(float value, const char *currency, char *out, size_t size) {
  if (std::strcmp(currency, "KRW")) { tickerFormat(value, out, size); return; }
  if (!std::isfinite(value) || std::fabs(value) > 1000000000.0f) {
    std::snprintf(out, size, "--"); return;
  }
  const uint32_t units = static_cast<uint32_t>(double(std::fabs(value)) + 0.5);
  std::snprintf(out, size, "%s%lu", value < 0 && units ? "-" : "",
      static_cast<unsigned long>(units));
}
