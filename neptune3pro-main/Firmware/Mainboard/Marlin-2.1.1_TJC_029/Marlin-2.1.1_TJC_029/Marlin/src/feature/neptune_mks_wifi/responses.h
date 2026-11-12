// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdio.h>

namespace NeptuneMksWifi {
inline void temperature_response(char *out, const size_t capacity, const int hotend, const int hotend_target,
                                 const int bed, const int bed_target, const bool acknowledge) {
  // ElegooSlicer TCPConsole completes a command only on a standalone "ok".
  // Keep unsolicited M991 telemetry free of acknowledgements.
  snprintf(out, capacity, "T:%d /%d B:%d /%d\r\n%s", hotend, hotend_target, bed, bed_target,
           acknowledge ? "ok\r\n" : "");
}
} // namespace NeptuneMksWifi
