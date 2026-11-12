#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace NeptuneMksWifi;
int main() {
  Decoder d; Frame f = {};
  const uint8_t packet[] = {0xA5,1,3,0,0xA5,0xFC,0,0xFC};
  for (size_t i=0;i<sizeof(packet);++i)
    assert(d.feed(packet[i],f)==(i+1==sizeof(packet)?FeedResult::frame:FeedResult::pending));
  assert(f.size==3 && f.data[0]==0xA5 && f.data[1]==0xFC);
  assert(d.feed(0xA5,f)==FeedResult::pending);
  d.feed(1,f); d.feed(0xFF,f); assert(d.feed(0xFF,f)==FeedResult::invalid);
  const uint8_t empty[]={0xA5,0,0,0,0xFC};
  for (int j=0;j<2;++j) for(size_t i=0;i<sizeof(empty);++i)
    assert(d.feed(empty[i],f)==(i==4?FeedResult::frame:FeedResult::pending));
  d.feed(0xA5,f); d.feed(1,f); d.feed(0,f); d.feed(0,f);
  assert(d.feed(0xA5,f)==FeedResult::invalid);
  d.feed(0,f); d.feed(0,f); d.feed(0,f); assert(d.feed(0xFC,f)==FeedResult::frame);
  uint8_t huge[1024]={0xA5,1,0xFB,3}; huge[1023]=0xFC;
  for (size_t i=0;i<sizeof(huge);++i)
    assert(d.feed(huge[i],f)==(i==1023?FeedResult::frame:FeedResult::pending));
  assert(f.size==1019);
  Transfer t; char name[13];
  uint8_t start[]={8,3,0,0,0,'T','E','S','T','.','G','C','O'};
  Frame first={2,start,sizeof(start)};
  assert(t.begin(first,name,sizeof(name))); assert(!strcmp(name,"TEST.GCO"));
  assert(!t.begin(first,name,sizeof(name)));
  uint8_t part[]={0,0,0,0,'G','1'}; Frame chunk={3,part,sizeof(part)};
  assert(t.validate(chunk)); assert(t.received()==0); assert(t.commit(chunk));
  assert(!t.validate(chunk));
  uint8_t end[]={1,0,0,0x80,10}; Frame last={3,end,sizeof(end)};
  assert(t.commit(last)); assert(!t.active() && t.received()==3);
  assert(!t.commit(last));
  assert(t.begin(first,name,sizeof(name)));
  uint8_t early[]={0,0,0,0x80,'G'}; Frame shortfile={3,early,sizeof(early)};
  assert(!t.validate(shortfile));
  uint8_t over[]={0,0,0,0x80,1,2,3,4}; Frame overflow={3,over,sizeof(over)};
  assert(!t.validate(overflow)); t.cancel();
  start[5]='/'; assert(!t.begin(first,name,sizeof(name))); start[5]='T';
  start[12]='X'; assert(!t.begin(first,name,sizeof(name))); start[12]='O';
  assert(!t.begin(first,name,8));
  for (uint16_t n=0;n<6;++n) { Frame shortstart={2,start,n}; assert(!t.begin(shortstart,name,sizeof(name))); }
  uint32_t random = 1;
  for (unsigned i=0; i<200000; ++i) {
    random = random * 1664525UL + 1013904223UL;
    if (d.feed(uint8_t(random >> 24),f)==FeedResult::frame) assert(f.size<=max_payload);
  }
  d.reset();
  for(size_t i=0;i<sizeof(empty);++i) d.feed(empty[i],f);
  assert(f.type==0 && f.size==0);
  puts("PASS: framing, binary delimiters, capacity, resynchronization, transfer order, length, filename restrictions");
}
