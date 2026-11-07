#pragma once
#include <cstdint>

// Independent API schedules; unsigned elapsed time remains safe at millis wrap.
struct ApiRefresh {
  explicit ApiRefresh(uint32_t refreshInterval = 15UL * 60UL * 1000UL)
      : successInterval(refreshInterval) {}

  uint32_t completedAt = 0;
  uint32_t interval = 0;
  uint8_t failures = 0;
  bool attempted = false;
  uint32_t successInterval;
  bool due(uint32_t now) const {
    return !attempted || now - completedAt >= interval;
  }
  void complete(uint32_t now, bool success) {
    attempted = true;
    completedAt = now;
    if (success) {
      failures = 0;
      interval = successInterval;
    } else {
      if (failures < 4) ++failures;
      interval = failures == 1 ? 30000UL : failures == 2 ? 60000UL
          : failures == 3 ? 120000UL : 300000UL;
    }
  }
};
