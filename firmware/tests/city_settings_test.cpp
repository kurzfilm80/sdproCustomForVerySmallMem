#include <cassert>
#include <cstring>
#include <iostream>
#include "CitySettings.h"

int main() {
  CitySettings defaults;
  assert(!strcmp(defaults.city, "SEOUL"));
  assert(!strcmp(defaults.latitude, "37.5665"));
  assert(!strcmp(defaults.longitude, "126.9780"));
  char output[12];
  assert(normalizeCoordinate("37.5665", 90, output));
  assert(!strcmp(output, "37.566500"));
  assert(normalizeCoordinate("-180.000000", 180, output));
  assert(!strcmp(output, "-180.000000"));
  assert(normalizeCoordinate("+90", 90, output));
  assert(!strcmp(output, "90.000000"));
  assert(normalizeCoordinate("-.5", 90, output));
  assert(!strcmp(output, "-0.500000"));
  assert(normalizeCoordinate("-0", 180, output));
  assert(!strcmp(output, "0.000000"));
  assert(normalizeCoordinate(".000001", 90, output));
  assert(!strcmp(output, "0.000001"));
  for (const char *input : {"", "+", "-", ".", "90.000001", "-90.000001", "91",
       "NaN", "inf", "1e2", "0.0000001", "37&evil=1", " 37", "37 ", "99999999999"}) {
    strcpy(output, "unchanged");
    assert(!normalizeCoordinate(input, 90, output));
    assert(!strcmp(output, "unchanged"));
  }
  assert(!normalizeCoordinate("180.000001", 180, output));
  assert(cityNameValid("MY CITY"));
  assert(cityNameValid("12345678901234567890"));
  for (const char *name : {"", " leading", "trailing ", "123456789012345678901", "line\nbreak", "서울"})
    assert(!cityNameValid(name));
  std::cout << "PASS: coordinate limits, precision, normalization, invalid input and display names\n";
}
