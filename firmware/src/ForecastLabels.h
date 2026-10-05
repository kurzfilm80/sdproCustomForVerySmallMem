#pragma once
#include <cstdint>

inline const char *forecastDayLabel(uint8_t index) {
  return index == 0 ? "D1" : index == 1 ? "D2" : "D3";
}
inline const char *displayWeekdayName(int currentWeekday, uint8_t dayIndex) {
  static const char *const names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  if (currentWeekday < 0 || currentWeekday > 6) return "---";
  return names[(dayIndex + currentWeekday) % 7];
}
