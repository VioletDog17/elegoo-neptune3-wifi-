#include "../neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029/Marlin/src/feature/neptune_mks_wifi/upload.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace NeptuneMksWifi;
struct Store {
  bool fail_open = false, fail_write = false, fail_finish = false;
  bool temporary = false, published = false;
  std::vector<uint8_t> bytes;
  bool create(const char *, char *alias) {
    if (fail_open) return false;
    strcpy(alias,"N1234567.GCO"); bytes.clear(); temporary = true; published = false; return true;
  }
  bool append(const uint8_t *data, uint16_t size) {
    if (fail_write) return false;
    bytes.insert(bytes.end(), data, data + size); return true;
  }
  bool finish(uint32_t expected) {
    if (fail_finish || expected != bytes.size()) return false;
    temporary = false; published = true; return true;
  }
  void cancel() { if (temporary) bytes.clear(); temporary = false; }
};
static std::vector<uint8_t> offer(const char *name, uint32_t size) {
  const size_t n = strlen(name);
  std::vector<uint8_t> p(n+5); p[0]=uint8_t(n);
  for (int i=0;i<4;++i) p[1+i]=uint8_t(size>>(8*i));
  memcpy(p.data()+5,name,n); return p;
}
static Frame first(std::vector<uint8_t> &p) { return {2,p.data(),uint16_t(p.size())}; }
static std::vector<uint8_t> fragment(const uint8_t *data, size_t count, uint32_t id, bool last) {
  std::vector<uint8_t> p(count+4);
  if(last) id|=0x80000000UL;
  for(int i=0;i<4;++i) p[i]=uint8_t(id>>(8*i));
  if(count) memcpy(p.data()+4,data,count);
  return p;
}
static Frame part(std::vector<uint8_t> &p) { return {3,p.data(),uint16_t(p.size())}; }
int main() {
  // Real MKS packetization: 1015-byte bodies, each frame padded to 1024,
  // and a zero-byte LAST packet for files divisible by the block size.
  for (const size_t size : {size_t(1),size_t(54),size_t(1014),size_t(1015),size_t(1016),size_t(2030),size_t(100000)}) {
    Store store; Upload<Store> upload(store); Decoder decoder;
    auto start=offer("sample long filename (1).gcode",uint32_t(size));
    assert(upload.begin(first(start)) && upload.active());
    assert(!upload.resolve("sample long filename (1).gcode"));
    assert(!upload.begin(first(start))); // does not destroy the active transfer
    std::vector<uint8_t> original(size);
    for(size_t i=0;i<size;++i) original[i]=uint8_t(i*37); // includes A5, FC, NUL
    size_t sent=0; uint32_t id=0;
    do {
      size_t count=size-sent; if(count>1015) count=1015;
      const bool last=count<1015;
      auto payload=fragment(count?original.data()+sent:nullptr,count,id++,last);
      std::vector<uint8_t> wire(1024,0);
      wire[0]=0xA5; wire[1]=3; wire[2]=uint8_t(payload.size()); wire[3]=uint8_t(payload.size()>>8);
      memcpy(wire.data()+4,payload.data(),payload.size()); wire[4+payload.size()]=0xFC;
      bool decoded=false;
      FragmentBlock block;
      const size_t previous_size = store.bytes.size();
      for(size_t i=0;i<wire.size();++i) {
        if (block.pending()) {
          assert(block.padding_byte(wire[i]));
          if (block.ready()) assert(block.commit(upload));
        }
        else {
          Frame f={};
          if(decoder.feed(wire[i],f)==FeedResult::frame) {
            assert(!decoded); assert(block.begin(f)); decoded=true;
            if(block.ready()) assert(block.commit(upload));
            else assert(!block.commit(upload)); // cannot touch SD early
          }
        }
        if (i != 1023) assert(store.bytes.size() == previous_size);
      }
      assert(decoded); sent+=count;
      assert(!store.published && !upload.complete());
      if(last) break;
    } while(true);
    assert(upload.ready() && upload.received()==size);
    assert(upload.finish() && store.published && !upload.active());
    assert(store.bytes==original);
    assert(upload.resolve("1:/sample long filename (1).gcode"));
    assert(upload.resolve("/N1234567.GCO"));
    assert(!upload.resolve("other.gcode"));
  }
  for (const char *bad : {"../a.gcode","a/b.gcode","a\\b.gcode","a;M24.gcode","a\".gcode","a.bin",".gcode","a..gco","a.gcode "})
    assert(!valid_upload_name(bad,strlen(bad)));
  Store store; Upload<Store> u(store);
  auto start=offer("test.gcode",3); auto p=fragment(reinterpret_cast<const uint8_t *>("G1\n"),3,0,true);
  store.fail_open=true; assert(!u.begin(first(start)) && !u.complete()); store.fail_open=false;
  assert(u.begin(first(start))); store.fail_write=true;
  assert(!u.fragment(part(p)) && !u.active() && !store.temporary && !u.complete()); store.fail_write=false;
  assert(u.begin(first(start))); assert(u.fragment(part(p))); store.fail_finish=true;
  assert(!u.finish() && !store.published && !store.temporary); store.fail_finish=false;
  assert(u.begin(first(start))); auto wrong=fragment(nullptr,0,1,true);
  assert(!u.fragment(part(wrong)) && !u.active());
  assert(u.begin(first(start))); auto early=fragment(reinterpret_cast<const uint8_t *>("G"),1,0,true);
  assert(!u.fragment(part(early)) && !u.complete());
  assert(u.begin(first(start))); u.cancel(); assert(!u.active() && !store.temporary);
  assert(u.begin(first(start))); assert(u.fragment(part(p))); assert(u.finish());
  auto invalid=offer("bad.bin",3); assert(!u.begin(first(invalid)) && !u.complete());
  const char *russian = u8"EN3Pro_0.4_Куб_PETG_FAST_0.2_33m51s.gcode";
  auto russian_start = offer(russian,3);
  assert(u.begin(first(russian_start)) && u.fragment(part(p)) && u.finish());
  assert(u.resolve(russian) && !strcmp(u.resolve(russian),"N1234567.GCO"));
  assert(!u.resolve(u8"EN3Pro_0.4_Шар_PETG_FAST_0.2_33m51s.gcode"));
  assert(valid_upload_name(u8"测试.gcode",strlen(u8"测试.gcode")));
  for (const char *bad_utf8 : {"\xC0\xAF.gcode", "\xED\xA0\x80.gcode", "\xF4\x90\x80\x80.gcode", "\xD0.gcode", "\x80.gcode"})
    assert(!valid_upload_name(bad_utf8,strlen(bad_utf8)));
  puts("upload tests PASS: real packet boundaries, exact bytes, long-name mapping, busy, write/finalize failures, cancellation, invalid filenames");
}
