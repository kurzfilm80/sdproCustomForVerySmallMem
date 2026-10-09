#include <cassert>
#include "WifiRetry.h"

int main() {
  using Action = WifiRetryAction;
  assert(wifiRetryAction(100000, 0, false, 0, 3, false) == Action::None);
  assert(wifiRetryAction(19999, 0, true, 1, 3, false) == Action::None);
  assert(wifiRetryAction(20000, 0, true, 1, 3, false) == Action::Retry);
  assert(wifiRetryAction(40000, 20000, true, 2, 3, false) == Action::Retry);
  assert(wifiRetryAction(60000, 40000, true, 3, 3, false) == Action::StartRecovery);
  assert(wifiRetryAction(119999, 60000, true, 3, 3, true) == Action::None);
  assert(wifiRetryAction(120000, 60000, true, 3, 3, true) == Action::Retry);
  assert(wifiRetryAction(180000, 120000, true, 3, 3, true) == Action::Retry);
  // A connection healthy until 1 hour does not inherit the boot-time deadline.
  assert(wifiRetryAction(3600100, 3600000, true, 0, 3, false) == Action::None);
  assert(wifiRetryAction(3620000, 3600000, true, 0, 3, false) == Action::Retry);
  // Unsigned subtraction preserves retry deadlines across millis() rollover.
  const uint32_t beforeWrap = UINT32_MAX - 9999;
  assert(wifiRetryAction(9999, beforeWrap, true, 1, 3, false) == Action::None);
  assert(wifiRetryAction(10000, beforeWrap, true, 1, 3, false) == Action::Retry);
  assert(wifiRetryAction(50000, beforeWrap, true, 3, 3, true) == Action::Retry);
}
