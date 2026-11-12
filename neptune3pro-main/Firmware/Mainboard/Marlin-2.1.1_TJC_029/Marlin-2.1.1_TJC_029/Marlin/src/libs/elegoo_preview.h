// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <string.h>
namespace ElegooPreview {
enum class Result { skip, data, complete, invalid };
struct Record { Result result; uint16_t begin, length; bool final; };
inline Record parse(const uint8_t *block, const uint16_t size, const bool started) {
  const bool first = size >= 8 && !memcmp(block, ";simage:", 8);
  const bool last = size >= 9 && !memcmp(block, ";;simage:", 9);
  if (started && size >= 6 && !memcmp(block, ";00000", 6)) return {Result::complete, 0, 0, true};
  if (!first && !(started && last)) return {Result::skip, 0, 0, false};
  const uint16_t begin = last ? 9 : 8;
  uint16_t end = size < 1023 ? size : 1023;
  for (uint16_t i = begin; i < end; ++i)
    if (block[i] == '\r' || block[i] == '\n' || !block[i]) { end = i; break; }
  for (uint16_t i = begin; i < end; ++i)
    if (block[i] < 32 || block[i] > 126 || block[i] == '"' || block[i] == '\\') return {Result::invalid, 0, 0, false};
  return {Result::data, begin, uint16_t(end - begin), last};
}
}
