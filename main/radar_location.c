// =============================================================================
// radar_location.c — NVS persistence for radar location and user preferences
// =============================================================================

#include "radar_location.h"
#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "location";

void radar_location_load(radar_location_t *out)
{
    out->lat       = (float)DEFAULT_RADAR_LAT;
    out->lon       = (float)DEFAULT_RADAR_LON;
    out->use_miles = false;
    out->range_idx = RANGE_PRESET_DEFAULT_IDX;

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    int32_t v;
    if (nvs_get_i32(h, "lat_e6", &v) == ESP_OK) out->lat = v / 1e6f;
    if (nvs_get_i32(h, "lon_e6", &v) == ESP_OK) out->lon = v / 1e6f;

    uint8_t u;
    if (nvs_get_u8(h, "miles", &u) == ESP_OK) out->use_miles = (bool)u;
    if (nvs_get_u8(h, "range", &u) == ESP_OK) out->range_idx = u;

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded: lat=%.4f lon=%.4f miles=%d range=%d",
             out->lat, out->lon, out->use_miles, out->range_idx);
}

void radar_location_save(const radar_location_t *loc)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_i32(h, "lat_e6", (int32_t)(loc->lat * 1e6f));
    nvs_set_i32(h, "lon_e6", (int32_t)(loc->lon * 1e6f));
    nvs_set_u8(h, "miles",  (uint8_t)loc->use_miles);
    nvs_set_u8(h, "range",  loc->range_idx);
    nvs_commit(h);
    nvs_close(h);
}

void radar_location_erase(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Location/prefs erased");
}
