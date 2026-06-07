// =============================================================================
// radar_location.h — Persist / load the radar centre location and preferences
// =============================================================================
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   lat;
    float   lon;
    bool    use_miles;   // display units preference
    uint8_t range_idx;  // active kRangePresets index
} radar_location_t;

/**
 * Load saved location/prefs from NVS.  Returns defaults if not saved.
 */
void radar_location_load(radar_location_t *out);

/**
 * Persist location/prefs to NVS.
 */
void radar_location_save(const radar_location_t *loc);

/**
 * Erase saved location/prefs (used by factory-reset).
 */
void radar_location_erase(void);

#ifdef __cplusplus
}
#endif
