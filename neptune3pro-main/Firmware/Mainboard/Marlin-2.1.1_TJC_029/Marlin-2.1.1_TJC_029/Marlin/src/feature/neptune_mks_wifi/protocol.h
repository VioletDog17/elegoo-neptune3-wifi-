// SPDX-License-Identifier: GPL-3.0-or-later
// UI-independent MKS UART framing. See lcd/extui/mks_ui/wifi_module.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace NeptuneMksWifi {
// The stock MKS receiver uses a 1024-byte buffer including the 5-byte envelope.
static const uint16_t max_payload = 1019;
enum class FeedResult : uint8_t { pending, frame, invalid };
struct Frame { uint8_t type; const uint8_t *data; uint16_t size; };

class Decoder {
  uint8_t payload_[max_payload];
  uint16_t used_ = 0, length_ = 0;
  uint8_t state_ = 0, type_ = 0;
public:
  void reset() { state_ = 0; used_ = length_ = 0; }
  // Returned payload remains valid until the next feed. Consume synchronously.
  FeedResult feed(const uint8_t byte, Frame &out) {
    switch (state_) {
      case 0: if (byte == 0xA5) state_ = 1; break;
      case 1:
        if (byte > 4) { reset(); if (byte == 0xA5) state_ = 1; return FeedResult::invalid; }
        type_ = byte; state_ = 2; break;
      case 2: length_ = byte; state_ = 3; break;
      case 3:
        length_ |= uint16_t(byte) << 8;
        if (length_ > max_payload) { reset(); return FeedResult::invalid; }
        used_ = 0; state_ = length_ ? 4 : 5; break;
      case 4:
        payload_[used_++] = byte;
        if (used_ == length_) state_ = 5;
        break;
      case 5:
        if (byte != 0xFC) { reset(); if (byte == 0xA5) state_ = 1; return FeedResult::invalid; }
        out = { type_, payload_, length_ }; reset(); return FeedResult::frame;
    }
    return FeedResult::pending;
  }
};

inline uint32_t read_le32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

// Validation only: no hardware, SD writes, motion or automatic print start.
// Commit a fragment only after the storage layer successfully writes it.
class Transfer {
  uint32_t expected_ = 0, received_ = 0, next_ = 0;
  bool active_ = false;
public:
  void cancel() { active_ = false; }
  bool active() const { return active_; }
  uint32_t received() const { return received_; }
  bool begin(const Frame &f, char *name, const size_t capacity) {
    if (active_ || f.type != 2 || f.size < 6) return false;
    const uint8_t n = f.data[0];
    if (!n || f.size != uint16_t(n) + 5 || capacity <= n) return false;
    // Restrict the first implementation to an explicit 8.3 G-code filename.
    // Do not silently rename: the slicer may subsequently request that name.
    uint8_t dot = 0;
    for (uint8_t i = 0; i < n; ++i) {
      const uint8_t c = f.data[5 + i];
      if (c == '.') { if (!i || dot || i > 8) return false; dot = i; }
      else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    if (!dot || n - dot < 2 || n - dot > 4) return false;
    char ext[4] = {};
    for (uint8_t i = dot + 1; i < n; ++i) {
      char c = char(f.data[5 + i]);
      ext[i - dot - 1] = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
    }
    if (ext[0] != 'G' || (ext[1] && !(ext[1] == 'C' && (!ext[2] || ext[2] == 'O')))) return false;
    const uint32_t size = read_le32(f.data + 1);
    if (!size) return false;
    for (uint8_t i = 0; i < n; ++i) name[i] = char(f.data[5 + i]);
    name[n] = 0; expected_ = size; received_ = next_ = 0; active_ = true;
    return true;
  }
  bool validate(const Frame &f) const {
    if (!active_ || f.type != 3 || f.size < 4) return false;
    const uint32_t id = read_le32(f.data), count = f.size - 4;
    if ((id & 0x7FFFFFFFUL) != next_ || count > expected_ - received_) return false;
    const bool last = (id & 0x80000000UL) != 0;
    return last == (received_ + count == expected_) && (count || last);
  }
  bool commit(const Frame &f) {
    if (!validate(f)) return false;
    received_ += f.size - 4; ++next_;
    if (read_le32(f.data) & 0x80000000UL) active_ = false;
    return true;
  }
};
} // namespace NeptuneMksWifi
