// =============================================================================
// config.h — ESP32-P4 Plane Radar
//
// Hardware: Waveshare ESP32-P4-WIFI6-Touch-LCD-4C
//   • Main SoC   : ESP32-P4  (RISC-V dual-core 360 MHz, 32 MB PSRAM)
//   • WiFi/BT    : ESP32-C6-MINI  (2.4 GHz Wi-Fi 6, BLE 5)
//                  Connected to P4 via 4-bit SDIO (ESP-Hosted transport)
//   • Display    : 4" IPS round, 720×720, MIPI-DSI 2-lane
//   • Touch      : Capacitive (GT911 or similar), I2C
//
// Original project: MatixYo/ESP32-Plane-Radar (ESP32-C3 + 1.28" GC9A01 SPI)
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
// Display geometry
// ---------------------------------------------------------------------------
#define DISPLAY_WIDTH       720
#define DISPLAY_HEIGHT      720
#define DISPLAY_ROTATION    0   // 0 = portrait / no rotation needed for a round

// ---------------------------------------------------------------------------
// MIPI-DSI — 2-lane, 720×720, JD9365DA or compatible panel
// The Waveshare board wires these internally; we configure via esp_lcd APIs.
// ---------------------------------------------------------------------------
#define DSI_LANE_NUM        2
#define DSI_LANE_MBPS       500     // per-lane bit rate in Mbps

// ---------------------------------------------------------------------------
// Touch (GT911) — I2C
// Waveshare uses I2C port 0 on the P4 with these pins.
// ---------------------------------------------------------------------------
#define TOUCH_I2C_PORT      0
#define TOUCH_I2C_SDA       GPIO_NUM_7
#define TOUCH_I2C_SCL       GPIO_NUM_8
#define TOUCH_I2C_ADDR      0x14    // GT911 default (alt: 0x5D)
#define TOUCH_RST           GPIO_NUM_NC   // managed by board
#define TOUCH_INT           GPIO_NUM_NC

// ---------------------------------------------------------------------------
// BOOT / user button
// GPIO_NUM_0 is the BOOT button on the Waveshare P4 board header.
// ---------------------------------------------------------------------------
#define BOOT_PIN            GPIO_NUM_0
#define BOOT_RESET_HOLD_MS  3000   // hold to clear saved WiFi/location
#define BOOT_TAP_MIN_MS     50     // debounce

// ---------------------------------------------------------------------------
// WiFi setup portal (captive portal via ESP-Hosted + lwIP)
// ---------------------------------------------------------------------------
#define PORTAL_AP_NAME      "PlaneRadar-Setup"
#define PORTAL_AP_IP        "192.168.4.1"
#define PORTAL_HOSTNAME     "plane-radar"    // -> plane-radar.local via mDNS
#define PORTAL_TIMEOUT_S    0                // 0 = no timeout

// ---------------------------------------------------------------------------
// NVS namespace
// ---------------------------------------------------------------------------
#define NVS_NAMESPACE       "planeradar"

// ---------------------------------------------------------------------------
// Default radar location (overridden via setup portal)
// Fuquay-Varina, NC as a reasonable US default; user should change.
// ---------------------------------------------------------------------------
#define DEFAULT_RADAR_LAT   35.5843
#define DEFAULT_RADAR_LON  -78.7997

// ---------------------------------------------------------------------------
// ADS-B data source  (opendata.adsb.fi — free, no key required)
// ---------------------------------------------------------------------------
#define ADSB_API_BASE       "https://opendata.adsb.fi/api/v2/lat/%s/lon/%s/dist/%d"
#define ADSB_FETCH_INTERVAL_MS  5000
#define ADSB_SHOW_GROUND    false   // hide ground-stationary aircraft

// ---------------------------------------------------------------------------
// Radar range presets  (km, shown on ring-3 label)
// Outer radius ≈ 4/3 of the ring-3 value.
// ---------------------------------------------------------------------------
// { ring3_km, ring3_mi_str, label_km_str, label_mi_str }
// Stored index persists in NVS.
#define RANGE_PRESET_DEFAULT_IDX  1   // 10 km default

// ---------------------------------------------------------------------------
// LVGL task
// ---------------------------------------------------------------------------
#define LVGL_TASK_STACK_SIZE    (16 * 1024)
#define LVGL_TASK_PRIORITY      5
#define LVGL_TASK_CORE          1           // pin LVGL to core 1
#define LVGL_TICK_PERIOD_MS     5

// ---------------------------------------------------------------------------
// Display backlight (PWM via LEDC — GPIO may vary by variant)
// Check your specific board revision schematic.
// ---------------------------------------------------------------------------
#define LCD_BL_PIN          GPIO_NUM_NC   // Some variants control BL via DSI
#define LCD_BL_LEDC_CH      0
#define LCD_BL_FREQ_HZ      5000
