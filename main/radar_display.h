// =============================================================================
// radar_display.h — Sonar-style radar drawing on the 720×720 LVGL canvas
// =============================================================================
#pragma once
#include "adsb_client.h"
#include "radar_location.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the radar LVGL screen (call once after display_init).
 */
void radar_display_init(void);

/**
 * @brief Load the radar screen as the active display.
 */
void radar_display_show(void);

/**
 * @brief Redraw the radar with the given aircraft and location data.
 *        Must be called with display_lock() held.
 */
void radar_display_update(const aircraft_t *aircraft, int count,
                           const radar_location_t *loc);

#ifdef __cplusplus
}
#endif
