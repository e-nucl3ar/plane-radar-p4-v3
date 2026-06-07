// =============================================================================
// adsb_client.c — ADS-B fetch via ESP-IDF esp_http_client + cJSON
// =============================================================================

#include "adsb_client.h"
#include "config.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "adsb";

// Maximum response buffer (bytes).  ADS-B responses can be large near airports.
#define RESP_BUF_SIZE   (64 * 1024)

// ---------------------------------------------------------------------------
// HTTP response accumulator
// ---------------------------------------------------------------------------
typedef struct {
    char  *buf;
    int    len;
    int    cap;
} http_buf_t;

static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
    http_buf_t *b = (http_buf_t *)evt->user_data;
    if (!b) return ESP_OK;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (b->len + evt->data_len < b->cap - 1) {
            memcpy(b->buf + b->len, evt->data, evt->data_len);
            b->len += evt->data_len;
            b->buf[b->len] = '\0';
        } else {
            ESP_LOGW(TAG, "Response buffer overflow — truncating");
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        break;
    default:
        break;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
int adsb_fetch(float lat, float lon, float radius_km,
               aircraft_t *out, int max_count)
{
    char url[256];
    char lat_s[16], lon_s[16];
    snprintf(lat_s, sizeof(lat_s), "%.4f", lat);
    snprintf(lon_s, sizeof(lon_s), "%.4f", lon);
    snprintf(url, sizeof(url), ADSB_API_BASE, lat_s, lon_s, (int)radius_km);

    char *buf = malloc(RESP_BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "No memory for HTTP buffer");
        return 0;
    }
    buf[0] = '\0';

    http_buf_t resp = { .buf = buf, .len = 0, .cap = RESP_BUF_SIZE };

    esp_http_client_config_t cfg = {
        .url              = url,
        .event_handler    = http_event_cb,
        .user_data        = &resp,
        .timeout_ms       = 4000,
        .buffer_size      = 4096,
        .buffer_size_tx   = 512,
        .transport_type   = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP fetch failed: %s", esp_err_to_name(err));
        free(buf);
        return 0;
    }

    // Parse JSON: {"ac":[{"flight":"UAL123","t":"B738","lat":35.1,"lon":-78.5,
    //              "alt_geom":32000,"track":270,"gs":450,"gnd":false}, ...]}
    int count = 0;
    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return 0;
    }

    cJSON *ac_arr = cJSON_GetObjectItem(root, "ac");
    if (!cJSON_IsArray(ac_arr)) {
        cJSON_Delete(root);
        return 0;
    }

    cJSON *ac;
    cJSON_ArrayForEach(ac, ac_arr) {
        if (count >= max_count) break;

        aircraft_t *a = &out[count];
        memset(a, 0, sizeof(*a));

        cJSON *j;
        j = cJSON_GetObjectItem(ac, "flight");
        if (cJSON_IsString(j))
            strlcpy(a->callsign, j->valuestring, sizeof(a->callsign));

        j = cJSON_GetObjectItem(ac, "t");
        if (cJSON_IsString(j))
            strlcpy(a->type, j->valuestring, sizeof(a->type));

        j = cJSON_GetObjectItem(ac, "lat");
        if (cJSON_IsNumber(j)) a->lat = (float)j->valuedouble;
        else continue;   // lat required

        j = cJSON_GetObjectItem(ac, "lon");
        if (cJSON_IsNumber(j)) a->lon = (float)j->valuedouble;
        else continue;

        j = cJSON_GetObjectItem(ac, "alt_geom");
        if (cJSON_IsNumber(j)) a->alt_ft = (int32_t)j->valueint;

        j = cJSON_GetObjectItem(ac, "track");
        if (cJSON_IsNumber(j)) a->heading_deg = (float)j->valuedouble;

        j = cJSON_GetObjectItem(ac, "gs");
        if (cJSON_IsNumber(j)) a->speed_kts = (float)j->valuedouble;

        j = cJSON_GetObjectItem(ac, "gnd");
        a->on_ground = cJSON_IsTrue(j);

        if (a->on_ground && !ADSB_SHOW_GROUND) continue;

        a->valid = true;
        count++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Fetched %d aircraft (radius %.0f km)", count, radius_km);
    return count;
}
