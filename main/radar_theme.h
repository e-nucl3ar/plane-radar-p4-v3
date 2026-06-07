// =============================================================================
// radar_theme.h — Radar display colours and geometry
//
// Adapted from MatixYo/ESP32-Plane-Radar for 720×720 resolution.
// All pixel values scaled from the original 240×240 by factor ≈ 3.
// =============================================================================
#pragma once
#include "lvgl.h"

// ---------------------------------------------------------------------------
// Colours  (LVGL lv_color_make(r, g, b))
// ---------------------------------------------------------------------------
#define RADAR_BG_COLOR          lv_color_make(0x05, 0x08, 0x20)  // deep navy
#define RADAR_RING_COLOR        lv_color_make(0x18, 0x60, 0x30)  // subdued green
#define RADAR_CROSS_COLOR       lv_color_make(0x18, 0x60, 0x30)
#define RADAR_LABEL_COLOR       lv_color_make(0xFF, 0xFF, 0xFF)  // white
#define RADAR_RANGE_COLOR       lv_color_make(0xA0, 0xFF, 0xA0)  // light green
#define RADAR_CENTER_COLOR      lv_color_make(0xFF, 0xFF, 0xFF)
#define AIRCRAFT_ICON_COLOR     lv_color_make(0xFF, 0x30, 0x30)  // red
#define AIRCRAFT_VECTOR_COLOR   lv_color_make(0xFF, 0x00, 0xFF)  // magenta
#define AIRCRAFT_TAG_COLOR      lv_color_make(0xFF, 0xFF, 0xFF)
#define RIM_DOT_COLOR           lv_color_make(0xFF, 0x30, 0x30)

// ---------------------------------------------------------------------------
// Geometry (pixels, for a 720×720 canvas)
// ---------------------------------------------------------------------------
#define RADAR_CENTER_X          360
#define RADAR_CENTER_Y          360
#define RADAR_OUTER_RADIUS      340   // leaves a small bezel margin
#define RADAR_RING_COUNT        4     // concentric rings
#define RADAR_RING_STROKE       2     // px
#define RADAR_CROSS_STROKE      1

// Cardinal label font size (pixels) — using LVGL built-in font
#define RADAR_COMPASS_FONT      (&lv_font_montserrat_28)
#define RADAR_RANGE_FONT        (&lv_font_montserrat_20)
#define RADAR_TAG_FONT          (&lv_font_montserrat_16)

// Aircraft triangle half-size (px)
#define AIRCRAFT_TRIANGLE_SIZE  18
// Speed vector max length (px) — clipped at outer ring
#define AIRCRAFT_VECTOR_LEN     80
// Center dot radius
#define RADAR_CENTER_DOT_R      5
// Rim dot radius (for off-screen aircraft)
#define RIM_DOT_R               8

// Status screen colours
#define STATUS_BG_COLOR         lv_color_make(0x10, 0x10, 0x10)
#define STATUS_TEXT_COLOR       lv_color_make(0xFF, 0xFF, 0xFF)
#define STATUS_WARN_COLOR       lv_color_make(0xFF, 0xCC, 0x00)  // yellow
#define STATUS_FONT             (&lv_font_montserrat_28)
#define STATUS_FONT_SMALL       (&lv_font_montserrat_20)
