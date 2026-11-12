// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <string.h>

namespace GcodeFilename {
inline char upper(const char c) { return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c; }
// Returns the basename length, excluding a recognized final extension, or zero.
inline size_t stem_length(const char *name) {
  if (!name || !*name) return 0;
  const char *dot = strrchr(name, '.');
  if (!dot || dot == name) return 0;
  const char *ext = dot + 1;
  const size_t n = strlen(ext);
  if ((n == 1 && upper(ext[0]) == 'G')
      || (n == 2 && upper(ext[0]) == 'G' && upper(ext[1]) == 'C')
      || (n == 3 && upper(ext[0]) == 'G' && upper(ext[1]) == 'C' && upper(ext[2]) == 'O')
      || (n == 5 && upper(ext[0]) == 'G' && upper(ext[1]) == 'C' && upper(ext[2]) == 'O' && upper(ext[3]) == 'D' && upper(ext[4]) == 'E'))
    return size_t(dot - name);
  return 0;
}
inline void display_name(char *out, const size_t capacity, const char *long_name, const char *short_name) {
  if (!capacity) return;
  const char *name = long_name && *long_name ? long_name : short_name;
  const size_t stem = stem_length(name);
  const size_t copied = stem < capacity ? stem : capacity - 1;
  if (copied) memcpy(out, name, copied);
  out[copied] = 0;
  if (stem >= capacity && capacity >= 4) { out[capacity - 3] = '.'; out[capacity - 2] = '.'; }
}
} // namespace GcodeFilename
