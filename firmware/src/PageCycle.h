#pragma once
#include <cstdint>

enum class ForecastPage : uint8_t { Weather, AirQuality };
struct PageCycle {
  ForecastPage page = ForecastPage::Weather;
  uint32_t shownAt = 0;
  void begin(uint32_t now) { page = ForecastPage::Weather; shownAt = now; }
  bool advance(uint32_t now) {
    const uint32_t duration = page == ForecastPage::Weather ? 12000 : 8000;
    if (now - shownAt < duration) return false;
    page = page == ForecastPage::Weather ? ForecastPage::AirQuality : ForecastPage::Weather;
    shownAt = now;
    return true;
  }
};
