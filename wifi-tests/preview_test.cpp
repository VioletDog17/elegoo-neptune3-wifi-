#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/libs/elegoo_preview.h"
#include <assert.h>
#include <stdio.h>
using namespace ElegooPreview;
int main(int argc, char **argv) {
  uint8_t b[1024]; memset(b, '0', sizeof(b));
  memcpy(b, ";simage:ABC", 11); b[1023]='\r';
  auto r=parse(b,1024,false); assert(r.result==Result::data && r.begin==8 && r.length==1015 && !r.final);
  memcpy(b,";;simage:LAST\r;00000",20);
  r=parse(b,1024,true); assert(r.result==Result::data && r.final && r.length==4);
  assert(parse(b,1024,false).result==Result::skip);
  memcpy(b,";simage:A\"B",11); assert(parse(b,1024,false).result==Result::invalid);
  memcpy(b,";simage:A\\B",11); assert(parse(b,1024,false).result==Result::invalid);
  assert(parse(b,3,false).result==Result::skip);
  memcpy(b,";00000",6); assert(parse(b,1024,true).result==Result::complete);
  assert(parse(b,1024,false).result==Result::skip);
  if (argc > 1) {
    FILE *f=fopen(argv[1], "rb"); assert(f);
    bool started=false, complete=false; unsigned records=0, bytes=0;
    for (unsigned offset=0; offset<102400; offset+=1024) {
      const size_t n=fread(b,1,sizeof(b),f); if (!n) break;
      r=parse(b,uint16_t(n),started); assert(r.result!=Result::invalid);
      if (r.result==Result::complete) { complete=true; break; }
      if (r.result==Result::data) {
        started=true; ++records; bytes+=r.length;
        if (r.final) { complete=true; break; }
      }
    }
    fclose(f); assert(complete && records && bytes);
    printf("Real file preview: %u records, %u encoded bytes, complete\n",records,bytes);
  }
  puts("preview tests PASS: complete/truncated records, final padding, command escaping, missing start");
}
