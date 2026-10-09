#pragma once
#include <cstdint>

enum class WifiRetryAction { None, Retry, StartRecovery };
inline WifiRetryAction wifiRetryAction(uint32_t now, uint32_t lastAttempt,
                                      bool configured, uint8_t attempts,
                                      uint8_t limit, bool recoveryRunning) {
  if (!configured) return WifiRetryAction::None;
  // Keep the recovery portal available without continuously scanning channels.
  const uint32_t interval = recoveryRunning ? 60000UL : 20000UL;
  if (now - lastAttempt < interval) return WifiRetryAction::None;
  if (!recoveryRunning && attempts >= limit) return WifiRetryAction::StartRecovery;
  return WifiRetryAction::Retry;
}
