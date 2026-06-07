// =============================================================================
// jd9365da_init_cmds.h — Vendor init sequence for the JD9365DA panel used in
// the Waveshare ESP32-P4-WIFI6-Touch-LCD-4C (720×720 round display).
//
// Source: Waveshare GitHub demo code for the 4C variant.
// https://github.com/waveshare/ESP32-P4-WIFI6-Touch-LCD-4C
//
// NOTE: Waveshare has not published the full init sequence in open-source form
// as of this writing.  You MUST copy the actual init sequence from the
// Waveshare demo project or BSP component for your specific panel.
//
// Steps:
//   1. Clone/download the Waveshare demo from the wiki:
//         https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C
//      or their GitHub repo:
//         https://github.com/waveshare/ESP32-P4-Touch-LCD-4C-Demo
//
//   2. Locate the panel init command table (usually in a file like
//      `jd9365da.c` or `esp_lcd_jd9365da.c`).
//
//   3. Copy the vendor_cmds array here, or better: use the component directly
//      from the ESP Component Registry if Waveshare or Espressif publishes one.
//         idf.py add-dependency "espressif/esp_lcd_jd9365da"
//
// Placeholder structure so the project compiles; replace with real data:
// =============================================================================
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t cmd;
    uint8_t data[32];
    uint8_t data_bytes;   // 0xFF = delay (data[0] = ms)
} jd9365da_cmd_t;

// Replace this entire table with the actual Waveshare init sequence.
static const jd9365da_cmd_t jd9365da_init_cmds[] = {
    // {cmd, {data...}, num_bytes}
    // TODO: populate from Waveshare demo / BSP
    {0xFF, {0x00}, 0},   // placeholder — display will NOT light up without real cmds
};
