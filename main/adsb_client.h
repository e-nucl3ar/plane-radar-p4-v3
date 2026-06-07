// =============================================================================
// adsb_client.h — ADS-B aircraft data fetcher
//
// Fetches live aircraft from opendata.adsb.fi and populates the aircraft list.
// Identical data model to the original project; HTTP client updated for
// ESP-IDF esp_http_client API.
// =============================================================================
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADSB_MAX_AIRCRAFT   80   // more room on the bigger display

typedef struct {
    char    callsign[10];
    char    type[6];       // ICAO type designator e.g. "B738"
    float   lat;
    float   lon;
    int32_t alt_ft;        // geometric altitude, feet
    float   heading_deg;   // true track
    float   speed_kts;     // ground speed
    bool    on_ground;
    bool    valid;
} aircraft_t;

/**
 * @brief Fetch aircraft around the given position within radius_km.
 *
 * Fills the provided array (max_count entries).  Returns the number of
 * aircraft populated.  Thread-safe; may block for the HTTP round-trip.
 *
 * @param lat        Radar center latitude
 * @param lon        Radar center longitude
 * @param radius_km  Fetch radius in kilometres
 * @param out        Output array of aircraft_t
 * @param max_count  Size of the out array
 * @return Number of aircraft written into out (0 on error or no traffic).
 */
int adsb_fetch(float lat, float lon, float radius_km,
               aircraft_t *out, int max_count);

#ifdef __cplusplus
}
#endif
