#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/receiver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace NeptuneMksWifi;
int main() {
  Receiver r; Frame f = {};
  r.feed(0xA5, 0, f); r.feed(1, 0, f); r.feed(10, 0, f);
  assert(!r.expire(999)); assert(r.expire(1000)); assert(!r.expire(1001));
  const uint8_t packet[] = {0xA5, 1, 5, 0, 'M', '1', '0', '5', '\n', 0xFC};
  for (size_t i = 0; i < sizeof(packet); ++i)
    assert(r.feed(packet[i], 1001 + uint32_t(i), f) == (i + 1 == sizeof(packet) ? FeedResult::frame : FeedResult::pending));
  assert(f.size == 5 && !memcmp(f.data, "M105\n", 5));
  assert(!r.expire(9999));
  // Unsigned elapsed arithmetic across millis() wraparound.
  r.feed(0xA5, 0xFFFFFF00UL, f);
  assert(!r.expire(0x000002E7UL)); assert(r.expire(0x000002E8UL));
  r.feed(0xA5, 0, f); r.feed(1, 0, f);
  // Timeout also works on the first new byte, without a separate expire call.
  for (size_t i = 0; i < sizeof(packet); ++i) r.feed(packet[i], 2000 + uint32_t(i), f);
  assert(f.size == 5);
  CommandLine line;
  for (const char c : "M105\r") if (c) assert(line.feed(c) == LineResult::pending);
  assert(line.feed('\n') == LineResult::line); assert(!strcmp(line.text(), "M105"));
  for (const char c : "M115") if (c) line.feed(c);
  assert(line.feed('\n') == LineResult::line); assert(!strcmp(line.text(), "M115"));
  for (int i = 0; i < 96; ++i) line.feed('X');
  for (const char c : "M105") if (c) line.feed(c);
  assert(line.feed('\n') == LineResult::invalid);
  for (const char c : "M997") if (c) line.feed(c);
  assert(line.feed('\n') == LineResult::line); assert(!strcmp(line.text(), "M997"));
  line.feed('M'); line.feed(0); line.feed('1');
  assert(line.feed('\n') == LineResult::invalid);
  line.feed('X'); line.reset(); line.feed('Y');
  assert(line.feed('\n') == LineResult::line && !strcmp(line.text(), "Y"));
  const char *utf8_command = u8"M23 EN3Pro_0.4_Куб_PETG_FAST_0.2_33m51s.gcode";
  for (const char *p = utf8_command; *p; ++p) assert(line.feed(uint8_t(*p)) == LineResult::pending);
  assert(line.feed('\n') == LineResult::line && !strcmp(line.text(), utf8_command));
  line.feed('M'); line.feed(0xD0); // truncated UTF-8 at command end
  assert(line.feed('\n') == LineResult::invalid);
  puts("receiver tests PASS");
}
