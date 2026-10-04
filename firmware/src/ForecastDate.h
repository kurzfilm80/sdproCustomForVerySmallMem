#pragma once
#include <cstring>

struct ForecastDate { int year = 0; int month = 0; int day = 0; };

inline int daysInMonth(int year, int month) {
  static const unsigned char lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month < 1 || month > 12) return 0;
  return lengths[month - 1] + (month == 2 && year % 4 == 0 &&
                             (year % 100 != 0 || year % 400 == 0));
}

inline bool parseForecastDate(const char *text, ForecastDate &out) {
  if (!text || strlen(text) < 10 || text[4] != '-' || text[7] != '-') return false;
  for (int i = 0; i < 10; ++i) {
    if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9')) return false;
  }
  ForecastDate value{(text[0]-'0')*1000+(text[1]-'0')*100+(text[2]-'0')*10+text[3]-'0',
                     (text[5]-'0')*10+text[6]-'0', (text[8]-'0')*10+text[9]-'0'};
  if (value.year < 1 || value.month < 1 || value.month > 12 || value.day < 1 ||
      value.day > daysInMonth(value.year, value.month)) return false;
  out = value;
  return true;
}

inline ForecastDate nextForecastDate(ForecastDate value) {
  if (++value.day > daysInMonth(value.year, value.month)) {
    value.day = 1;
    if (++value.month > 12) { value.month = 1; ++value.year; }
  }
  return value;
}

inline void formatForecastDate(char out[11], ForecastDate value) {
  out[0]='0'+value.year/1000%10; out[1]='0'+value.year/100%10;
  out[2]='0'+value.year/10%10; out[3]='0'+value.year%10; out[4]='-';
  out[5]='0'+value.month/10; out[6]='0'+value.month%10; out[7]='-';
  out[8]='0'+value.day/10; out[9]='0'+value.day%10; out[10]='\0';
}

inline int forecastWeekday(ForecastDate value) {
  static const unsigned char offsets[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  int year = value.year - (value.month < 3);
  return (year + year/4 - year/100 + year/400 + offsets[value.month-1] + value.day) % 7;
}

inline const char *forecastWeekdayName(ForecastDate value) {
  static const char *const names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  return names[forecastWeekday(value)];
}
