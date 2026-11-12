// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../sd/cardreader.h"
#include "upload.h"
#include "filename_metadata.h"
#include <stdio.h>

namespace NeptuneMksWifi {
class SdStore {
  SdFile root_, file_;
  char temporary_[13] = {};
  char final_[13] = {};
  char name_file_[13] = {}, original_[96] = {};
  bool name_owned_ = false;
  bool owned_ = false;
  static bool exists(SdFile &root, const char *name) {
    SdFile probe;
    if (!probe.open(&root, name, O_READ)) return false;
    probe.close(); return true;
  }
public:
  bool create(const char *original, char *alias) {
    if (!card.isMounted()) return false;
    root_ = card.getroot();
    // Unique 8.3 aliases: never truncate or overwrite an existing user file.
    for (uint16_t attempt = 0; attempt < 256; ++attempt) {
      storage_name(original, attempt, final_, sizeof(final_));
      memcpy(temporary_, final_, sizeof(final_));
      memcpy(strrchr(temporary_, '.') + 1, "TMP", 4);
      name_sidecar(final_,name_file_);
      if (exists(root_, final_) || exists(root_, temporary_) || exists(root_,name_file_)) continue;
      if (!file_.open(&root_, temporary_, O_CREAT | O_EXCL | O_WRITE)) return false;
      owned_ = true; name_owned_ = false;
      strcpy(original_,original);
      strcpy(alias, final_);
      return true;
    }
    return false;
  }
  bool append(const uint8_t *data, const uint16_t size) {
    return card.isMounted() && owned_ && file_.write(data, size) == size && !file_.writeError;
  }
  bool finish(const uint32_t size) {
    if (!owned_ || !card.isMounted() || file_.fileSize() != size || file_.writeError || !file_.sync()) return false;
    if (!file_.rename(&root_, final_)) return false;
    // If close fails, remove our newly published file instead of leaving it printable.
    if (!file_.close()) { SdFile::remove(&root_, final_); return false; }
    uint8_t data[NAME_RECORD_CAPACITY];
    const size_t bytes=encode_name(data,original_,size);
    SdFile metadata;
    if (!bytes || !metadata.open(&root_,name_file_,O_CREAT | O_EXCL | O_WRITE)) return false;
    name_owned_=true;
    const bool written=metadata.write(data,bytes)==int16_t(bytes) && !metadata.writeError && metadata.sync();
    const bool closed=metadata.close();
    if (!written || !closed) return false;
    owned_ = name_owned_ = false;
    return true;
  }
  void cancel() {
    if (!owned_) return;
    file_.close();
    if (card.isMounted()) {
      SdFile::remove(&root_, temporary_);
      // Both names were absent when create() reserved this pair. A failed
      // rename may already have created the destination directory entry.
      SdFile::remove(&root_, final_);
      if (name_owned_) SdFile::remove(&root_,name_file_);
    }
    owned_ = name_owned_ = false;
  }
};
} // namespace NeptuneMksWifi
