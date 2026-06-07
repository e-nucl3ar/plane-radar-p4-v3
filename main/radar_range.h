// =============================================================================
// radar_range.h — Range presets for the 720×720 radar display
//
// Identical semantics to the original project; the outer fetch radius is
// ~4/3 of the ring-3 label so off-screen aircraft appear as rim dots.
// =============================================================================
#pragma once
#include <stdint.h>

typedef struct {
    uint16_t ring3_km;          // range shown on the east spoke label
    float    outer_km;          // actual fetch radius (= ring3_km * 4/3)
    const char *label_km;       // e.g. "10 km"
    const char *label_mi;       // e.g. "6 mi"
} radar_range_preset_t;

static const radar_range_preset_t kRangePresets[] = {
    {  5,  6.67f, " 5 km", " 3 mi" },
    { 10, 13.33f, "10 km", " 6 mi" },
    { 15, 20.00f, "15 km", " 9 mi" },
    { 25, 33.33f, "25 km", "16 mi" },
    { 50, 66.67f, "50 km", "31 mi" },   // bonus extra range for the larger screen
};
#define RANGE_PRESET_COUNT  (sizeof(kRangePresets) / sizeof(kRangePresets[0]))

// Returns outer fetch radius in km for the given preset index.
static inline float radar_range_fetch_km(uint8_t idx) {
    if (idx >= RANGE_PRESET_COUNT) idx = 1;
    return kRangePresets[idx].outer_km;
}

// Returns ring-3 radius in km (for scale calculations).
static inline float radar_range_ring3_km(uint8_t idx) {
    if (idx >= RANGE_PRESET_COUNT) idx = 1;
    return (float)kRangePresets[idx].ring3_km;
}
