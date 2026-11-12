// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace NeptuneMksWifi {
inline bool printable_utf8(const char *data, const size_t size) {
  for (size_t i = 0; i < size;) {
    const uint8_t first = uint8_t(data[i++]);
    if (first < 0x80) {
      if (first < 0x20 || first == 0x7F) return false;
      continue;
    }
    uint8_t remaining;
    uint32_t code, minimum;
    if (first >= 0xC2 && first <= 0xDF) { remaining = 1; code = first & 0x1F; minimum = 0x80; }
    else if (first >= 0xE0 && first <= 0xEF) { remaining = 2; code = first & 0x0F; minimum = 0x800; }
    else if (first >= 0xF0 && first <= 0xF4) { remaining = 3; code = first & 7; minimum = 0x10000; }
    else return false;
    if (size - i < remaining) return false;
    while (remaining--) {
      const uint8_t next = uint8_t(data[i++]);
      if ((next & 0xC0) != 0x80) return false;
      code = (code << 6) | (next & 0x3F);
    }
    if (code < minimum || code < 0xA0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
  }
  return true;
}
} // namespace NeptuneMksWifi
