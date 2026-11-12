// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.h"
#include "utf8.h"
#include <string.h>
#include <stdio.h>

namespace NeptuneMksWifi {
// Decoder payload stays valid while raw padding is consumed without feed().
// No SD write is allowed until all 1024 UART bytes have arrived. In particular,
// allocating the first SD cluster while a short final packet is still arriving
// can lose UART bytes on this board.
class FragmentBlock {
  Frame frame_ = {};
  uint16_t remaining_ = 0;
  bool pending_ = false;
public:
  void reset() { pending_ = false; remaining_ = 0; }
  bool pending() const { return pending_; }
  bool ready() const { return pending_ && !remaining_; }
  bool begin(const Frame &frame) {
    if (pending_ || frame.type != 3 || frame.size < 4 || frame.size > max_payload) return false;
    frame_ = frame; remaining_ = 1024 - (frame.size + 5); pending_ = true;
    return true;
  }
  bool padding_byte(const uint8_t byte) {
    if (!pending_ || !remaining_ || byte) return false;
    --remaining_; return true;
  }
  template<class Session> bool commit(Session &session) {
    if (!ready()) return false;
    const Frame frame = frame_; reset();
    return session.fragment(frame);
  }
};
inline char upper(const char c) { return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c; }
inline bool same_name(const char *a, const char *b) {
  while (*a && upper(*a) == upper(*b)) { ++a; ++b; }
  return !*a && !*b;
}
inline void storage_name(const char *original, const uint16_t attempt, char *out, const size_t capacity) {
  const char *dot = strrchr(original, '.');
  const size_t stem = dot ? size_t(dot - original) : 0;
  bool simple = stem && stem <= 8;
  for (size_t i = 0; i < stem && simple; ++i) {
    const char c = upper(original[i]);
    simple = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
  }
  if (simple && !attempt && capacity >= stem + 5) {
    for (size_t i = 0; i < stem; ++i) out[i] = upper(original[i]);
    memcpy(out + stem, ".GCO", 5);
    return;
  }
  uint32_t hash = 2166136261UL;
  for (const char *p = original; *p; ++p) hash = (hash ^ uint8_t(*p)) * 16777619UL;
  const uint32_t offset = simple && attempt ? attempt - 1 : attempt;
  snprintf(out, capacity, "N%07lX.GCO", (unsigned long)((hash + offset) & 0x0FFFFFFFUL));
}
inline bool valid_upload_name(const char *name, const size_t length) {
  // Leave room for "M23 " in the 96-byte command assembler.
  if (!length || length > 90 || name[0] == '.' || name[length - 1] == ' ' || !printable_utf8(name, length)) return false;
  size_t dot = length;
  for (size_t i = 0; i < length; ++i) {
    const char c = name[i];
    if (uint8_t(c) >= 0x80) continue; // UTF-8 sequence was validated above.
    if (c == '.') {
      if (i && name[i - 1] == '.') return false;
      dot = i;
    }
    else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ' || c == '(' || c == ')')) return false;
  }
  if (!dot || dot == length) return false;
  const char *ext = name + dot + 1;
  const size_t n = length - dot - 1;
  return (n == 1 && upper(ext[0]) == 'G') ||
         (n == 2 && upper(ext[0]) == 'G' && upper(ext[1]) == 'C') ||
         (n == 3 && upper(ext[0]) == 'G' && upper(ext[1]) == 'C' && upper(ext[2]) == 'O') ||
         (n == 5 && upper(ext[0]) == 'G' && upper(ext[1]) == 'C' && upper(ext[2]) == 'O' && upper(ext[3]) == 'D' && upper(ext[4]) == 'E');
}

// Store contract: create(original, alias[13]), append(data,size),
// finish(expected_size), cancel(). finish must sync and publish the temp file.
template<class Store> class Upload {
  Store &store_;
  Transfer transfer_;
  char original_[96] = {}, alias_[13] = {};
  uint32_t expected_ = 0;
  bool active_ = false, ready_ = false, complete_ = false;
public:
  explicit Upload(Store &store) : store_(store) {}
  bool active() const { return active_; }
  bool ready() const { return ready_; }
  bool complete() const { return complete_; }
  uint32_t received() const { return transfer_.received(); }
  const char *alias() const { return alias_; }
  const char *original() const { return original_; }
  void cancel() { store_.cancel(); transfer_.cancel(); active_ = ready_ = complete_ = false; }
  bool begin(const Frame &frame) {
    if (active_) return false;
    complete_ = ready_ = false;
    if (frame.type != 2 || frame.size < 6) return false;
    const uint8_t n = frame.data[0];
    if (frame.size != uint16_t(n) + 5 || !valid_upload_name(reinterpret_cast<const char *>(frame.data + 5), n)) return false;
    expected_ = read_le32(frame.data + 1);
    if (!expected_) return false;
    memcpy(original_, frame.data + 5, n); original_[n] = 0;
    if (!store_.create(original_, alias_)) return false;
    uint8_t mapped[18] = {12};
    memcpy(mapped + 1, frame.data + 1, 4);
    const uint8_t size = uint8_t(strlen(alias_));
    mapped[0] = size;
    memcpy(mapped + 5, alias_, size);
    const Frame offer = {2, mapped, uint16_t(size + 5)};
    if (!transfer_.begin(offer, alias_, sizeof(alias_))) { cancel(); return false; }
    active_ = true;
    return true;
  }
  bool fragment(const Frame &frame) {
    if (!active_ || ready_ || !transfer_.validate(frame)) { cancel(); return false; }
    const uint16_t size = frame.size - 4;
    if (size && !store_.append(frame.data + 4, size)) { cancel(); return false; }
    if (!transfer_.commit(frame)) { cancel(); return false; }
    ready_ = !transfer_.active();
    return true;
  }
  bool finish() {
    if (!active_ || !ready_ || !store_.finish(expected_)) { cancel(); return false; }
    active_ = ready_ = false;
    complete_ = true;
    return true;
  }
  const char *resolve(const char *requested) const {
    if (!complete_) return nullptr;
    if (!strncmp(requested, "0:/", 3) || !strncmp(requested, "1:/", 3)) requested += 3;
    else if (*requested == '/') ++requested;
    return same_name(requested, original_) || same_name(requested, alias_) ? alias_ : nullptr;
  }
};
} // namespace NeptuneMksWifi
