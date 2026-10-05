#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// Coordinates use six fixed decimal places, avoiding libc float parsing/printing.
inline bool parseCoordinate(const char *text, uint16_t limit, int32_t &result) {
  if (!text || !*text || std::strlen(text) > 11) return false;
  const bool negative = *text == '-';
  if (*text == '-' || *text == '+') ++text;
  uint32_t whole = 0, fraction = 0, scale = 1;
  bool digits = false;
  while (*text >= '0' && *text <= '9') {
    digits = true;
    whole = whole * 10 + (*text++ - '0');
    if (whole > limit) return false;
  }
  if (*text == '.') {
    ++text;
    while (*text >= '0' && *text <= '9') {
      if (scale == 1000000) return false;
      digits = true;
      fraction = fraction * 10 + (*text++ - '0');
      scale *= 10;
    }
  }
  if (!digits || *text || (whole == limit && fraction)) return false;
  const int32_t value = whole * 1000000 + fraction * (1000000 / scale);
  result = negative ? -value : value;
  return true;
}
