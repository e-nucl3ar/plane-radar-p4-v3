// =============================================================================
// display.h — MIPI-DSI display + LVGL initialisation
//
// Uses the waveshare/esp_lcd_dsi IDF component for panel bring-up.
// No vendor init-sequence header required — the component handles it.
// =============================================================================
#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise MIPI-DSI PHY (LDO), DSI bus, DPI panel, and LVGL.
 *
 * Call once before any lv_* API. Starts the LVGL tick timer and
 * handler task internally.
 */
esp_err_t display_init(void);

/** Acquire the LVGL mutex (required before calling lv_* from app tasks). */
void display_lock(void);

/** Release the LVGL mutex. */
void display_unlock(void);

/** Return the registered LVGL display handle. */
lv_disp_t *display_get(void);

/**
 * @brief Set backlight brightness 0–100 %.
 *        No-op when LCD_BL_PIN == GPIO_NUM_NC.
 */
void display_set_backlight(uint8_t pct);

#ifdef __cplusplus
}
#endif
