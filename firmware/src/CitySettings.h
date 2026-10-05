#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "DecimalParser.h"

struct CitySettings {
  char city[10] = "SEOUL";
  char name[21] = "SEOUL";
  char latitude[12] = "37.5665";
  char longitude[12] = "126.9780";
};

inline bool cityNameValid(const char *name) {
  const size_t length = name ? std::strlen(name) : 0;
  if (!length || length > 20 || name[0] == ' ' || name[length - 1] == ' ') return false;
  for (size_t i = 0; i < length; ++i)
    if (name[i] < 32 || name[i] > 126) return false;
  return true;
}

inline bool normalizeCoordinate(const char *input, uint16_t limit, char (&out)[12]) {
  int32_t value;
  if (!parseCoordinate(input, limit, value)) return false;
  const uint32_t magnitude = value < 0 ? -value : value;
  // Bound the integer field explicitly so compiler range analysis can prove
  // the longest result (-180.000000 plus terminator) fits this buffer.
  if (magnitude > 180000000UL) return false;
  std::snprintf(out, sizeof(out), "%s%u.%06u", value < 0 ? "-" : "",
                static_cast<unsigned>((magnitude / 1000000) % 1000),
                static_cast<unsigned>(magnitude % 1000000));
  return true;
}

bool selectPresetCity(const char *id, CitySettings &out);
bool saveCitySettings(const CitySettings &settings, bool filesystemReady);
void loadCitySettings(CitySettings &settings, bool filesystemReady);
