#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/libs/gcode_filename.h"
#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/responses.h"
#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/upload.h"
#include <assert.h>
#include <stdio.h>
#include <string>

// The slicer's TCPConsole trims a line and compares it to the literal "ok".
static bool slicer_done(const char *response) {
  std::string stream(response); size_t from = 0;
  for (size_t end; (end = stream.find('\n', from)) != std::string::npos; from = end + 1) {
    std::string line = stream.substr(from, end - from);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line == "ok") return true;
  }
  return false;
}
int main() {
  for (const char *name : {"M6x12.gcode", "M6X12.GCODE", "N536B3B9.GCO", "part.GcO", "part.g", "part.GC"})
    assert(GcodeFilename::stem_length(name));
  for (const char *name : {"", "a", ".gcode", "x.gcode.bak", "x.TMP", "x.bin", "x.gcoextra"})
    assert(!GcodeFilename::stem_length(name));
  char shown[20] = {};
  GcodeFilename::display_name(shown,sizeof(shown),"","N536B3B9.GCO");
  assert(!strcmp(shown,"N536B3B9")); // short names have no LFN entry on FAT
  GcodeFilename::display_name(shown,sizeof(shown),"M6x12.gcode","M6X12.GCO");
  assert(!strcmp(shown,"M6x12"));
  GcodeFilename::display_name(shown,sizeof(shown),"A very very very long model.gcode","LONG.GCO");
  assert(strlen(shown)==sizeof(shown)-1 && shown[17]=='.' && shown[18]=='.');
  assert(GcodeFilename::stem_length("Model.v2.gcode")==8);
  char name[13];
  NeptuneMksWifi::storage_name("M6x12.gcode",0,name,sizeof(name));
  assert(!strcmp(name,"M6X12.GCO"));
  NeptuneMksWifi::storage_name("M6x12.gcode",1,name,sizeof(name));
  assert(!strcmp(name,"N8AFF0FF.GCO")); // first collision falls back to a unique alias
  NeptuneMksWifi::storage_name("long model name.gcode",0,name,sizeof(name));
  assert(strlen(name)==12 && name[0]=='N' && !strcmp(name+8,".GCO"));
  char reply[96];
  NeptuneMksWifi::temperature_response(reply,sizeof(reply),25,0,26,0,true);
  assert(!strcmp(reply,"T:25 /0 B:26 /0\r\nok\r\n"));
  assert(slicer_done(reply));
  assert(!slicer_done("ok T:25 /0 B:26 /0\nT:25 /0 B:26 /0\n")); // previous infinite wait
  NeptuneMksWifi::temperature_response(reply,sizeof(reply),25,0,26,0,false);
  assert(!slicer_done(reply)); // periodic telemetry must not complete another command
  puts("compatibility tests PASS: TJC short/LFN names, extensions, collision names, standalone M105 acknowledgement");
}
