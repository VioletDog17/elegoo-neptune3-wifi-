// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "upload.h"
namespace NeptuneMksWifi {
constexpr size_t NAME_RECORD_CAPACITY = 107;
inline uint32_t name_checksum(const uint8_t *data, size_t size) {
  uint32_t sum = 2166136261UL;
  while (size--) sum = (sum ^ *data++) * 16777619UL;
  return sum;
}
inline void name_put32(uint8_t *out, uint32_t n) {
  for (uint8_t i=0; i<4; ++i) { out[i]=uint8_t(n); n >>= 8; }
}
inline size_t encode_name(uint8_t *out, const char *name, const uint32_t file_size) {
  const size_t n = strlen(name);
  if (!valid_upload_name(name,n)) return 0;
  const uint8_t magic[8] = {'N','3','N','A','M','E',1,0};
  memcpy(out,magic,8); name_put32(out+8,file_size); out[12]=uint8_t(n);
  memcpy(out+13,name,n); name_put32(out+13+n,name_checksum(out,13+n));
  return 17+n;
}
inline bool decode_name(const uint8_t *data, const size_t size, const uint32_t file_size,
                        char *out, const size_t capacity) {
  if (capacity) out[0]=0;
  const uint8_t magic[8] = {'N','3','N','A','M','E',1,0};
  if (size<17 || size>NAME_RECORD_CAPACITY || memcmp(data,magic,8)) return false;
  const size_t n=data[12];
  if (size!=17+n || capacity<=n || read_le32(data+8)!=file_size
      || read_le32(data+13+n)!=name_checksum(data,13+n)
      || !valid_upload_name(reinterpret_cast<const char *>(data+13),n)) return false;
  memcpy(out,data+13,n); out[n]=0; return true;
}
inline bool name_sidecar(const char *alias, char *out) {
  const size_t n=strlen(alias);
  if (n<5 || n>12 || strchr(alias,'/') || strchr(alias,'\\')) return false;
  const char *dot=strrchr(alias,'.');
  if (!dot || strlen(dot)!=4) return false;
  memcpy(out,alias,n+1); memcpy(out+(dot-alias)+1,"NAM",4); return true;
}
}
