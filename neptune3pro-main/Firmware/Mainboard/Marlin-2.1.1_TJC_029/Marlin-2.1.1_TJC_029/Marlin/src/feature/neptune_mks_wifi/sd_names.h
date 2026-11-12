// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../sd/cardreader.h"
#include "filename_metadata.h"
namespace NeptuneMksWifi {
inline bool read_uploaded_name(const char *alias, char *out, const size_t capacity) {
  if (capacity) out[0]=0;
  char metadata[13];
  if (!card.isMounted() || !name_sidecar(alias,metadata)) return false;
  SdFile root=card.getroot(), record, gcode;
  if (!record.open(&root,metadata,O_READ)) return false;
  uint8_t data[NAME_RECORD_CAPACITY];
  const uint32_t size=record.fileSize();
  const bool read_ok=size<=sizeof(data) && record.read(data,size)==int16_t(size);
  record.close();
  if (!read_ok || !gcode.open(&root,alias,O_READ)) return false;
  const uint32_t gcode_size=gcode.fileSize(); gcode.close();
  return decode_name(data,size,gcode_size,out,capacity);
}
inline bool resolve_uploaded_name(const char *requested, char *alias, uint32_t &size) {
  if (!card.isMounted()) return false;
  if (!strncmp(requested,"0:/",3) || !strncmp(requested,"1:/",3)) requested+=3;
  else if (*requested=='/') ++requested;
  SdFile root=card.getroot(), item;
  root.rewind();
  uint16_t budget=0; bool found=false;
  while (budget++<1024 && item.openNext(&root,O_READ)) {
    char candidate[13]={}, name[96]; item.getDosName(candidate);
    const bool valid=item.isFile() && valid_upload_name(candidate,strlen(candidate));
    const uint32_t candidate_size=item.fileSize(); item.close();
    if (valid && read_uploaded_name(candidate,name,sizeof(name))
        && (same_name(requested,candidate) || same_name(requested,name))) {
      if (same_name(requested,candidate)) { strcpy(alias,candidate); size=candidate_size; return true; }
      if (found) return false; // Repeated original names must never select an arbitrary print.
      strcpy(alias,candidate); size=candidate_size; found=true;
    }
  }
  return budget<=1024 && found;
}
}
