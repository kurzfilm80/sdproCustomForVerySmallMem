#include <cassert>
#include <cstring>
#include "ForecastLabels.h"
int main() {
  assert(!strcmp(forecastDayLabel(0), "D1"));
  assert(!strcmp(forecastDayLabel(1), "D2"));
  assert(!strcmp(forecastDayLabel(2), "D3"));
  const char *names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  for (int today = 0; today < 7; ++today)
    for (unsigned i = 0; i < 3; ++i)
      assert(!strcmp(displayWeekdayName(today, i), names[(today+i)%7]));
  assert(!strcmp(displayWeekdayName(-1, 0), "---"));
  assert(!strcmp(displayWeekdayName(-1, 2), "---"));
  assert(!strcmp(displayWeekdayName(7, 1), "---"));
}
