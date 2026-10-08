#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

constexpr uint8_t kTickerLimit = 8;
constexpr uint8_t kSparkLimit = 32;
struct TickerPosition {
  char symbol[24]{};
  float quantity = 0, cost = 0;
};
struct TickerSettings {
  TickerPosition positions[kTickerLimit]{};
  uint8_t count = 0;
  uint16_t rotateSeconds = 10;
  uint16_t refreshSeconds = 300;
};
struct TickerQuote {
  float price = 0, change = 0, percent = 0;
  float spark[kSparkLimit]{};
  char currency[8]{};
  uint8_t sparkCount = 0, failures = 0;
  bool valid = false, error = false, hasChange = false;
  uint32_t nextTry = 0, lastOk = 0;
  int httpCode = 0;
};
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
