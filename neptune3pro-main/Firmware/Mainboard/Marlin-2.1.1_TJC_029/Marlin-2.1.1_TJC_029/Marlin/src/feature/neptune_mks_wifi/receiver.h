// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.h"
#include "utf8.h"

namespace NeptuneMksWifi {
// Time out a truncated frame even if the sender never sends another byte.
class Receiver {
  Decoder decoder_;
  uint32_t last_ = 0;
  bool pending_ = false;
public:
  static const uint32_t timeout_ms = 1000;
  bool expire(const uint32_t now) {
    if (!pending_ || uint32_t(now - last_) < timeout_ms) return false;
    reset();
    return true;
  }
  void reset() { decoder_.reset(); pending_ = false; }
  FeedResult feed(const uint8_t byte, const uint32_t now, Frame &frame) {
    expire(now);
    const FeedResult result = decoder_.feed(byte, frame);
    pending_ = decoder_.pending();
    last_ = now;
    return result;
  }
};

enum class LineResult : uint8_t { pending, line, invalid };
// Preserve commands split across UART frames. Discard a malformed/overlong
// line through its newline, so its suffix can never become a new command.
class CommandLine {
  char data_[96] = {};
  uint8_t used_ = 0;
  bool discard_ = false;
public:
  void reset() { used_ = 0; discard_ = false; }
  const char *text() const { return data_; }
  LineResult feed(const uint8_t c) {
    if (c == '\n') {
      const bool invalid = discard_ || !printable_utf8(data_, used_);
      data_[used_] = 0;
      reset();
      return invalid ? LineResult::invalid : LineResult::line;
    }
    if (discard_) return LineResult::pending;
    if (c == '\r') return LineResult::pending;
    if (c < 32 || c == 127 || used_ == sizeof(data_) - 1) discard_ = true;
    else data_[used_++] = char(c);
    return LineResult::pending;
  }
};
} // namespace NeptuneMksWifi
