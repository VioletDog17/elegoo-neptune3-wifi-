// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/web_control.h"
using namespace NeptuneMksWifi;
int main() {
  WebCommand c;
  const char *invalid[]={"M996 0 HOME","M996 65536 HOME","M996 1 HOME Q","M996 1 JOG X nan","M996 1 JOG X inf","M996 1 JOG Z 10.1","M996 1 JOG X 0","M996 1 JOG X 1e2","M996 1 JOG X 2 G28","M996 1 NOZZLE 261","M996 1 BED 101","M996 1 FAN -1","M996 1 SPEED 201","M996 1 SPEED 9","M996 1 FAN 10.5","M996 1 COOLDOWN 0"};
  for(auto s:invalid)assert(!parse_web_command(s,c));
  assert(parse_web_command("M996 65535 JOG X -0.1",c));assert(c.id==65535&&c.axis=='X'&&c.value<0);
  assert(web_denied(c,{false,false,false,false,false}));
  assert(web_denied(c,{false,true,false,false,true}));
  assert(web_denied(c,{false,false,true,false,true}));
  assert(web_denied(c,{false,false,false,true,true}));
  assert(web_denied(c,{true,false,false,false,true}));
  assert(!web_denied(c,{false,false,false,false,true}));
  assert(parse_web_command("M996 1 HOME Z",c));assert(!web_denied(c,{false,false,false,false,false}));
  assert(parse_web_command("M996 2 FAN 100",c));assert(!web_denied(c,{false,true,false,true,true}));
  assert(parse_web_command("M996 3 COOLDOWN",c));assert(web_denied(c,{false,false,true,false,true}));
  assert(parse_web_command("M996 4 NOZZLE 260",c));assert(parse_web_command("M996 5 BED 0",c));
  WebJob job;
  assert(parse_web_job("M796 1 PRINT N1234567.GCO",job));assert(job.id==1);
  assert(parse_web_job("M796 65535 RESUME",job));
  for (auto s:{"M796 0 STOP","M796 65536 STOP","M796 1 PRINT ../X.GCO","M796 1 PRINT X.GCO M24","M796 1 STOP EXTRA","M796 1 PRINT A\nM24"}) assert(!parse_web_job(s,job));
  puts("web_control: strict parser, limits and operation gates passed");
}
