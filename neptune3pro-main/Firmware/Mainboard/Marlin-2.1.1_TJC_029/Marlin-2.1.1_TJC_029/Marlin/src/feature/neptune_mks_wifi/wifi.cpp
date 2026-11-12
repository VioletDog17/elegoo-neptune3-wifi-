// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../inc/MarlinConfig.h"
#if ENABLED(NEPTUNE_MKS_WIFI)

#if !MB(MKS_E3D_V2) || DISABLED(TJC_AVAILABLE) || DISABLED(RTS_AVAILABLE)
  #error "NEPTUNE_MKS_WIFI requires the Neptune MKS_E3D_V2 board and TJC display."
#endif
#if SERIAL_PORT == 2 || (defined(SERIAL_PORT_2) && SERIAL_PORT_2 == 2) || (defined(SERIAL_PORT_3) && SERIAL_PORT_3 == 2)
  #error "USART2 is reserved for binary MKS Wi-Fi traffic."
#endif
#if ENABLED(MKS_WIFI_MODULE)
  #error "Do not enable the LVGL MKS_WIFI_MODULE with NEPTUNE_MKS_WIFI."
#endif
#if ENABLED(EMERGENCY_PARSER)
  #error "Binary Wi-Fi UART must not pass through the text emergency parser."
#endif
#if LCD_SERIAL_PORT != 2 || LCD_SERIAL_PORT_2 != 6
  #error "Neptune diagnostic mapping requires USART2 Wi-Fi and USART6 TJC."
#endif
#if DISABLED(SDSUPPORT) || !HAS_HEATED_BED
  #error "This diagnostic build expects the stock Neptune SD and heated bed configuration."
#endif
#if SERIAL_RX_BUFFER_SIZE < 2048
  #error "Wi-Fi diagnostics require the 2048-byte UART RX buffer from the diagnostic environment."
#endif

#include "wifi.h"
#include "receiver.h"
#include "responses.h"
#include "../../module/temperature.h"
#include "../../sd/cardreader.h"
#include "../../gcode/queue.h"
#include "../../module/planner.h"
#include "../../module/printcounter.h"
#if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
  #include "upload.h"
  #include "sd_store.h"
  #include "sd_names.h"
  #include "../../lcd/extui/dgus/elegoo/DGUSDisplayDef.h"
#endif
#include <stdio.h>
#include <string.h>

#if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
extern bool sdcard_pause_check, CardUpdate;
extern int Update_Time_Value;
#endif

namespace NeptuneMksWifi {
static Receiver receiver;
static CommandLine command;
static bool in_tick = false;
static uint8_t network_state = 0;
static uint32_t network_seen = 0, screen_state_due = 0;
static uint32_t last_command_byte = 0;
static bool command_pending = false;
static volatile bool receiving_upload = false, rx_failed = false;
#if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
static SdStore store;
static Upload<SdStore> upload(store);
static FragmentBlock block;
static uint32_t last_upload_byte = 0, cooldown_until = 0;
static bool selected = false, start_pending = false, show_print_page = false;
static char selected_name[13] = {}, selected_display[96] = {};
static uint32_t completed_size = 0;
static char last_upload_error[48] = {};
#endif

bool storage_busy() {
  #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
    return upload.active();
  #else
    return false;
  #endif
}
void rx_byte() {
  // ESP samples READY between packets. Stop the next packet immediately,
  // even when the main loop is in a slow SD write or a screen update.
  if (receiving_upload) WRITE(PC15, HIGH);
}
void rx_fault() { rx_failed = true; }
void card_file_changed() {
  #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
    selected = start_pending = show_print_page = false;
  #endif
}

// Schematic rev V2.2_001, sheets 2 and 9. IO1 HIGH pauses the module.
static void send(const char *text) {
  const uint16_t size = strlen(text);
  MSerial2.write(uint8_t(0xA5));
  MSerial2.write(uint8_t(2)); // WIFI_TRANS_INF, printer -> ESP
  MSerial2.write(uint8_t(size));
  MSerial2.write(uint8_t(size >> 8));
  MSerial2.write(reinterpret_cast<const uint8_t *>(text), size);
  MSerial2.write(uint8_t(0xFC));
}

#if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
static void exception(const uint8_t code) {
  const uint8_t packet[] = {0xA5, 3, 1, 0, code, 0xFC};
  MSerial2.write(packet, sizeof(packet));
  MSerial2.flush();
}
static void baud(const uint32_t speed) {
  MSerial2.flush();
  MSerial2.end();
  MSerial2.begin(speed);
}
static void fail_upload(const char *reason) {
  strncpy(last_upload_error, reason, sizeof(last_upload_error) - 1);
  last_upload_error[sizeof(last_upload_error) - 1] = 0;
  WRITE(PC15, HIGH);
  exception(1); // Report at the current baud before ESP resets its UART.
  upload.cancel();
  receiving_upload = false;
  selected = start_pending = show_print_page = false;
  block.reset();
  baud(115200);
  receiver.reset(); command.reset(); command_pending = false; rx_failed = false;
  cooldown_until = millis() + 3000;
  SERIAL_ECHOLNPGM("MKS upload failed: ", reason);
}
static void finish_upload() {
  WRITE(PC15, HIGH);
  if (!upload.finish()) { fail_upload("SD sync/rename failed"); return; }
  completed_size = upload.received();
  CardUpdate = true;
  receiving_upload = false;
  baud(115200);
  receiver.reset(); command.reset(); command_pending = false;
  exception(2);
  SERIAL_ECHOLNPGM("MKS upload saved: ", upload.alias(), " bytes: ", completed_size);
  WRITE(PC15, LOW);
}
static bool idle_for_upload() {
  return !card.isFileOpen() && !card.flag.saving && !card.isPrinting()
      && !print_job_timer.isRunning() && !print_job_timer.isPaused()
      && !queue.has_commands_queued() && !planner.has_blocks_queued() && !start_pending;
}
static void commit_block() {
  if (!block.commit(upload)) { fail_upload("Bad fragment/order or SD write failed"); return; }
  if (upload.ready()) finish_upload();
  else WRITE(PC15, LOW);
}
static void upload_frame(const Frame &frame) {
  if (frame.type == 2) {
    selected = false;
    if (upload.active() || !idle_for_upload()) { fail_upload("Printer or SD busy"); return; }
    if (frame.size < 6 || frame.size != uint16_t(frame.data[0]) + 5) { fail_upload("Invalid file header"); return; }
    if (!valid_upload_name(reinterpret_cast<const char *>(frame.data + 5), frame.data[0])) { fail_upload("Filename invalid or exceeds 90 UTF-8 bytes"); return; }
    if (!read_le32(frame.data + 1)) { fail_upload("Empty file"); return; }
    if (!card.isMounted()) card.mount();
    if (!card.isMounted()) { fail_upload("No SD card"); return; }
    if (!upload.begin(frame)) { fail_upload("SD open failed or no free filename"); return; }
    last_upload_error[0] = 0;
    receiving_upload = true;
    rx_failed = false;
    last_upload_byte = millis();
    block.reset();
    baud(1958400);
    SERIAL_ECHOLNPGM("MKS upload receiving: ", upload.original());
    WRITE(PC15, LOW);
  }
  else if (frame.type == 3) {
    if (!block.begin(frame)) { fail_upload("Invalid fragment block"); return; }
    if (block.ready()) commit_block();
  }
}

static bool transfer_command(const char *line) {
  if (!strcmp(line, "M20") || !strcmp(line, "M20 0:/") || !strcmp(line, "M20 1:/")) {
    if (storage_busy() || card.isFileOpen()) { send("Error:SD busy\r\n"); return true; }
    if (!card.isMounted()) card.mount();
    if (!card.isMounted()) { send("Error:No SD card\r\n"); return true; }
    SdFile root = card.getroot(), file;
    root.rewind();
    send("Begin file list\r\n");
    uint16_t budget = 0;
    while (budget++ < 256 && file.openNext(&root, O_READ)) {
      char name[13] = {};
      file.getDosName(name);
      if (file.isFile() && valid_upload_name(name, strlen(name))) {
        char original[96], entry[100];
        const char *shown = read_uploaded_name(name,original,sizeof(original)) ? original : name;
        snprintf(entry, sizeof(entry), "%s\r\n", shown); send(entry);
      }
      file.close();
    }
    send("End file list\r\nok\r\n");
    return true;
  }
  if (!strcmp(line, "M21")) {
    if (!storage_busy()) { if (!card.isMounted()) card.mount(); send(card.isMounted() ? "ok\r\n" : "Error:No SD card\r\n"); }
    else send("Error:SD busy\r\n");
    return true;
  }
  if (!strncmp(line, "M23 ", 4)) {
    selected = false;
    if (storage_busy() || start_pending || card.isPrinting() || print_job_timer.isRunning() || print_job_timer.isPaused()) { send("Error:Printer busy\r\n"); return true; }
    const char *name = line + 4;
    while (*name == ' ') ++name;
    const char *resolved = upload.resolve(name);
    char persisted[13]; uint32_t expected_size=completed_size;
    if (!resolved) {
      if (!resolve_uploaded_name(name,persisted,expected_size)) { send("Error:No verified WiFi upload for this filename\r\n"); return true; }
      resolved=persisted;
    }
    card.openFileRead(resolved);
    if (!card.isFileOpen() || card.getFileSize() != expected_size) {
      if (card.isFileOpen()) card.closefile();
      send("Error:Uploaded file could not be selected\r\n"); return true;
    }
    strcpy(selected_name, resolved);
    if (!read_uploaded_name(resolved,selected_display,sizeof(selected_display)))
      strcpy(selected_display,upload.complete() && same_name(resolved,upload.alias()) ? upload.original() : resolved);
    selected = true;
    send("File selected\r\nok\r\n");
    return true;
  }
  if (!strcmp(line, "M24")) {
    if (storage_busy() || start_pending || !selected || !card.isFileOpen() || card.isPrinting()) { send("Error:No verified selected file\r\n"); return true; }
    RTS_LoadPrintPreview(selected_name);
    if (!queue.enqueue_one(F("M24"))) { send("Error:Command queue full\r\n"); return true; }
    start_pending = show_print_page = true;
    send("ok\r\n"); // Only after queue acceptance, never before SD finalization.
    return true;
  }
  if (!strcmp(line, "M25")) {
    if (!storage_busy() && card.isPrinting() && queue.enqueue_one(F("M25"))) send("ok\r\n");
    else send("Error:Pause not available\r\n");
    return true;
  }
  if (!strcmp(line, "M26")) {
    if (storage_busy()) fail_upload("Cancelled by host");
    else if (card.isFileOpen()) { card.abortFilePrintSoon(); selected = start_pending = show_print_page = false; send("ok\r\n"); }
    else send("Error:No active print\r\n");
    return true;
  }
  if (!strcmp(line, "M998")) {
    char text[96];
    snprintf(text, sizeof(text), "M998 %s %lu %s%s%s\r\nok\r\n", upload.complete() ? "SAVED" : upload.active() ? "RECEIVING" : "NONE", (unsigned long)upload.received(), upload.complete() ? upload.alias() : "-", last_upload_error[0] ? " ERROR:" : "", last_upload_error);
    send(text); return true;
  }
  return false;
}
#endif

static void execute(const char *line) {
  while (*line == ' ') ++line;
  const size_t size = strlen(line);
  if (!size) return;
  // No queue injection in the diagnostic build. Exact read-only commands only.
  char text[128];
  #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
    if (transfer_command(line)) return;
  #endif
  if (!strcmp(line, "M115"))
    #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
      send("FIRMWARE_NAME:Robin_Neptune3_MKS_UPLOAD\r\nok\r\n");
    #else
      send("FIRMWARE_NAME:Neptune3_MKS_DIAGNOSTIC\r\nok\r\n");
    #endif
  else if (!strcmp(line, "M105") || !strcmp(line, "M991")) {
    temperature_response(text, sizeof(text),
      int(thermalManager.wholeDegHotend(0)), int(thermalManager.degTargetHotend(0)),
      int(thermalManager.wholeDegBed()), int(thermalManager.degTargetBed()), !strcmp(line, "M105"));
    send(text);
  }
  else if (!strcmp(line, "M997"))
    send(card.isPaused() ? "M997 PAUSE\r\n" : card.isPrinting() ? "M997 PRINTING\r\n" : "M997 IDLE\r\n");
  else if (!strcmp(line, "M27")) {
    snprintf(text, sizeof(text), "M27 %u\r\n", unsigned(card.percentDone())); send(text);
  }
  else if (!strcmp(line, "M992")) {
    const uint32_t seconds = print_job_timer.duration();
    snprintf(text, sizeof(text), "M992 %02lu:%02lu:%02lu\r\n", (unsigned long)(seconds / 3600), (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60)); send(text);
  }
  else if (!strcmp(line, "M994")) {
    const char *name = card.isFileOpen() ? card.filename : "";
    #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
      char original[96];
      if (card.isFileOpen() && read_uploaded_name(name,original,sizeof(original))) name=original;
    #endif
    snprintf(text, sizeof(text), "M994 %s;%lu\r\n", name, (unsigned long)(card.isFileOpen() ? card.getFileSize() : 0)); send(text);
  }
  else
    send("Error:WiFi diagnostic mode; command disabled\r\n");
}

static void handle(const Frame &frame) {
  if (frame.type == 0) {
    // The rest contains variable-length SSID/key/cloud fields. Do not log keys.
    if (frame.size < 8) return;
    network_seen = millis();
    network_state = frame.data[6] == 0x0A ? 2 : 1;
    static uint8_t previous[5] = {};
    uint8_t status[5] = {frame.data[0], frame.data[1], frame.data[2], frame.data[3], frame.data[6]};
    if (memcmp(previous, status, sizeof(status))) {
      memcpy(previous, status, sizeof(status));
      SERIAL_ECHOLNPGM("MKS WiFi IP: ", int(status[0]), ".", int(status[1]), ".", int(status[2]), ".", int(status[3]), " state: ", int(status[4]));
    }
  }
  else if (frame.type == 1) {
    for (uint16_t i = 0; i < frame.size; ++i) {
      const LineResult result = command.feed(frame.data[i]);
      command_pending = result == LineResult::pending;
      last_command_byte = millis();
      if (result == LineResult::line) execute(command.text());
      else if (result == LineResult::invalid) send("Error:Invalid WiFi command\r\n");
    }
  }
  else if (frame.type == 2 || frame.type == 3)
    #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
      upload_frame(frame);
    #else
    send("Error:WiFi diagnostic mode; upload disabled\r\n");
    #endif
}

void init() {
  OUT_WRITE(PC15, HIGH); // WIFI_IO1: not ready during startup
  SET_INPUT_PULLUP(PA0); // WIFI_IO0: retain normal ESP boot strap
  OUT_WRITE(PA1, HIGH); // WIFI_CTRL: release reset, no blocking reset pulse
  MSerial2.begin(115200);
  receiver.reset();
  command.reset();
  WRITE(PC15, LOW);
  #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
    SERIAL_ECHOLNPGM("MKS WiFi upload receiver ready (USART2, 115200 -> 1958400)");
  #else
    SERIAL_ECHOLNPGM("MKS WiFi diagnostic receiver ready (USART2, 115200)");
  #endif
}

void tick() {
  if (in_tick) return;
  in_tick = true;
  const uint32_t now = millis();
  // Refresh even an unchanged value so a restarted display catches up.
  if (!storage_busy() && !MSerial2.available() && ELAPSED(now, screen_state_due)) {
    const uint8_t state = network_seen && uint32_t(now - network_seen) < 30000 ? network_state : 0;
    LCD_SERIAL_2.printf("wifi_state=%d\xff\xff\xff", state);
    screen_state_due = now + 2000;
  }
  #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
    if (cooldown_until && PENDING(now, cooldown_until)) { in_tick = false; return; }
    if (cooldown_until) { cooldown_until = 0; while (MSerial2.available()) MSerial2.read(); receiver.reset(); WRITE(PC15, LOW); }
    if (upload.active() && (rx_failed || !card.isMounted() || !IS_SD_INSERTED() || uint32_t(now - last_upload_byte) >= 10000)) {
      fail_upload("UART overflow/error, card removed or transfer timeout"); in_tick = false; return;
    }
    // A queued M24 may execute after this tick. Update the screen only once printing starts.
    if (show_print_page && card.isPrinting()) {
      show_print_page = start_pending = false;
      ::sdcard_pause_check = true;
      ::Update_Time_Value = 0;
      PoweroffContinue = true;
      LCD_SERIAL_2.print("printpause.t0.txt=\""); LCD_SERIAL_2.print(selected_display); LCD_SERIAL_2.print("\"\xff\xff\xff");
      LCD_SERIAL_2.print("page printpause\xff\xff\xff");
    }
    if (start_pending && !queue.has_commands_queued() && !card.isFileOpen()) selected = start_pending = show_print_page = false;
  #endif
  const bool frame_timeout = receiver.expire(now);
  #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
    if (frame_timeout && receiving_upload) { fail_upload("Truncated frame timeout"); in_tick = false; return; }
  #endif
  if (frame_timeout || (command_pending && uint32_t(now - last_command_byte) >= Receiver::timeout_ms)) {
    command.reset();
    command_pending = false;
  }
  // Bound work so heaters, the TJC screen and other idle tasks keep running.
  for (uint16_t budget = 0; budget < 2048 && MSerial2.available(); ++budget) {
    const uint8_t byte = uint8_t(MSerial2.read());
    #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
      if (receiving_upload) last_upload_byte = millis();
      if (block.pending()) {
        if (!block.padding_byte(byte)) { fail_upload("Nonzero block padding"); break; }
        if (block.ready()) commit_block();
        if (cooldown_until) break;
        continue;
      }
    #endif
    Frame frame = {};
    const FeedResult result = receiver.feed(byte, millis(), frame);
    if (result == FeedResult::frame) {
      WRITE(PC15, HIGH);
      handle(frame);
      #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
        if ((!receiving_upload || (frame.type != 2 && frame.type != 3)) && !cooldown_until) WRITE(PC15, LOW);
        if (cooldown_until) break;
      #else
        WRITE(PC15, LOW);
      #endif
    }
    else if (result == FeedResult::invalid) {
      #if ENABLED(NEPTUNE_MKS_WIFI_UPLOAD)
        if (receiving_upload) { fail_upload("Invalid UART frame"); break; }
      #endif
      command.reset();
      command_pending = false;
    }
  }
  in_tick = false;
}
} // namespace NeptuneMksWifi
#endif
