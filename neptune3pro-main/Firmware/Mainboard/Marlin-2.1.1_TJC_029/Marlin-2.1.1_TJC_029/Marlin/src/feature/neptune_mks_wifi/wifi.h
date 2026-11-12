// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace NeptuneMksWifi {
  void init();
  void tick();
  bool storage_busy();
  // IRQ hooks: no SD operations, allocation or serial output.
  void rx_byte();
  void rx_fault();
  void card_file_changed();
}
