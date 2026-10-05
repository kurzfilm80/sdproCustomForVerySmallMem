#include <cassert>
#include <initializer_list>
#include "ApiRefresh.h"
int main() {
  ApiRefresh weather, air;
  assert(weather.due(0));
  uint32_t now = 0xfffff000U;
  for (uint32_t pause : {30000U, 60000U, 120000U, 300000U, 300000U}) {
    weather.complete(now, false);
    assert(weather.interval == pause);
    assert(!weather.due(now + pause - 1));
    assert(weather.due(now + pause));
    now += pause;
  }
  assert(air.due(now));
  weather.complete(now, true);
  assert(weather.failures == 0 && weather.interval == 900000);
  assert(!weather.due(now + 899999));
  assert(weather.due(now + 900000));
  weather.complete(now, false);
  assert(weather.interval == 30000);
}
