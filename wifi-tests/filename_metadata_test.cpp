#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/filename_metadata.h"
#include <assert.h>
#include <stdio.h>
using namespace NeptuneMksWifi;
int main() {
  uint8_t record[NAME_RECORD_CAPACITY]; char out[96];
  const char *names[]={"M6x12.gcode",u8"Куб для принтера.gcode",u8"测试文件.gcode"};
  for (const char *name:names) {
    const size_t n=encode_name(record,name,20667242);
    assert(n && decode_name(record,n,20667242,out,sizeof(out)) && !strcmp(name,out));
    for (size_t cut=0;cut<n;++cut) assert(!decode_name(record,cut,20667242,out,sizeof(out)));
    assert(!decode_name(record,n,20667241,out,sizeof(out)));
    assert(!decode_name(record,n,20667242,out,strlen(name)));
    for (size_t i=0;i<n;++i) { record[i]^=1; assert(!decode_name(record,n,20667242,out,sizeof(out))); record[i]^=1; }
  }
  char longest[91]; memset(longest,'a',84); memcpy(longest+84,".gcode",7);
  size_t n=encode_name(record,longest,1); assert(n==NAME_RECORD_CAPACITY);
  assert(decode_name(record,n,1,out,sizeof(out)) && !strcmp(longest,out));
  assert(!encode_name(record,"../bad.gcode",1));
  assert(!encode_name(record,"bad\".gcode",1));
  assert(!encode_name(record,"bad\n.gcode",1));
  char sidecar[13]; assert(name_sidecar("NFDAA5BB.GCO",sidecar) && !strcmp(sidecar,"NFDAA5BB.NAM"));
  assert(!name_sidecar("../X.GCO",sidecar));
  assert(!name_sidecar("LONGFILENAME.GCO",sidecar));
  puts("filename metadata tests PASS: persistent UTF-8 roundtrip, corruption, truncation, file size, capacity, unsafe names");
}
