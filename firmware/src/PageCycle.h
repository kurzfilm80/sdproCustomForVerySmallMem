#pragma once
#include <cstdint>

enum class ForecastPage : uint8_t { Weather, AirQuality };
struct PageCycle {
  ForecastPage page = ForecastPage::Weather;
  uint32_t shownAt = 0;
  bool autoRotate = true;
  uint8_t weatherSeconds = 15;
  uint8_t airSeconds = 10;
  ForecastPage fixedPage = ForecastPage::Weather;
  void begin(uint32_t now) {
    page = autoRotate ? ForecastPage::Weather : fixedPage;
    shownAt = now;
  }
  bool configure(uint32_t now, bool rotate, uint8_t weather, uint8_t air, ForecastPage fixed) {
    autoRotate = rotate;
    weatherSeconds = weather;
    airSeconds = air;
    fixedPage = fixed;
    const ForecastPage previous = page;
    if (!autoRotate) page = fixedPage;
    shownAt = now;
    return previous != page;
  }
  bool advance(uint32_t now) {
    if (!autoRotate) return false;
    const uint32_t duration = uint32_t(page == ForecastPage::Weather ? weatherSeconds : airSeconds) * 1000;
    if (now - shownAt < duration) return false;
    page = page == ForecastPage::Weather ? ForecastPage::AirQuality : ForecastPage::Weather;
    shownAt = now;
    return true;
  }
};
