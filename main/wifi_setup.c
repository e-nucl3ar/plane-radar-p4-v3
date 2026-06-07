// =============================================================================
// wifi_setup.c — WiFi + captive portal for ESP32-P4
//
// Uses ESP-IDF native APIs (esp_wifi, esp_netif, esp_http_server, mdns).
// WiFi is transparent through the ESP-Hosted SDIO layer; from this code's
// perspective it behaves identically to a native WiFi chip.
// =============================================================================

#include "wifi_setup.h"
#include "config.h"
#include "radar_location.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "wifi";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define PORTAL_SAVED_BIT    BIT2

static EventGroupHandle_t s_wifi_events = NULL;
static bool s_portal_saved = false;
static radar_location_t s_portal_loc = {0};

// ---------------------------------------------------------------------------
// NVS helpers for credentials
// ---------------------------------------------------------------------------
#define NVS_WIFI_NS "wifi_creds"

static void creds_save(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid", ssid);
    nvs_set_str(h, "pass", pass);
    nvs_commit(h);
    nvs_close(h);
}

static bool creds_load(char ssid[64], char pass[64])
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t n = 64;
    bool ok = (nvs_get_str(h, "ssid", ssid, &n) == ESP_OK);
    n = 64;
    ok = ok && (nvs_get_str(h, "pass", pass, &n) == ESP_OK);
    nvs_close(h);
    return ok;
}

// ---------------------------------------------------------------------------
// WiFi event handler
// ---------------------------------------------------------------------------
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        ESP_LOGW(TAG, "WiFi disconnected");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupClearBits(s_wifi_events, WIFI_FAIL_BIT);
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

// ---------------------------------------------------------------------------
// Portal HTTP handlers
// ---------------------------------------------------------------------------

// Minimal embedded HTML setup page
static const char *SETUP_HTML =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Plane Radar Setup</title>"
    "<style>body{font-family:sans-serif;max-width:480px;margin:2em auto;}"
    "input{width:100%;padding:8px;margin:6px 0;box-sizing:border-box;}"
    "button{width:100%;padding:10px;background:#2a8;color:#fff;border:none;"
    "cursor:pointer;font-size:1em;}</style></head><body>"
    "<h2>&#9992; Plane Radar Setup</h2>"
    "<form method='POST' action='/save'>"
    "<label>WiFi SSID<input name='ssid' required></label>"
    "<label>WiFi Password<input name='pass' type='password'></label>"
    "<label>Latitude (decimal)<input name='lat' value='" XSTR(DEFAULT_RADAR_LAT) "' required></label>"
    "<label>Longitude (decimal)<input name='lon' value='" XSTR(DEFAULT_RADAR_LON) "' required></label>"
    "<label><input type='checkbox' name='miles'> Show distances in miles</label><br>"
    "<button type='submit'>Save &amp; Connect</button>"
    "</form></body></html>";

#define XSTR(s) STR(s)
#define STR(s) #s

static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, SETUP_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handler_save(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    // Simple URL-decode query parser (no full RFC compliance needed)
    char ssid[64] = {0}, pass[64] = {0}, lat_s[20] = {0}, lon_s[20] = {0};
    bool miles = (strstr(buf, "miles=") != NULL);

    // Extract fields (naive but sufficient for known form)
    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "pass", pass, sizeof(pass));
    httpd_query_key_value(buf, "lat",  lat_s, sizeof(lat_s));
    httpd_query_key_value(buf, "lon",  lon_s, sizeof(lon_s));

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    creds_save(ssid, pass);

    s_portal_loc.lat       = lat_s[0]  ? strtof(lat_s, NULL) : DEFAULT_RADAR_LAT;
    s_portal_loc.lon       = lon_s[0]  ? strtof(lon_s, NULL) : DEFAULT_RADAR_LON;
    s_portal_loc.use_miles = miles;
    s_portal_loc.range_idx = RANGE_PRESET_DEFAULT_IDX;
    radar_location_save(&s_portal_loc);

    const char *resp = "<html><body><h2>Saved! Rebooting…</h2></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    s_portal_saved = true;
    if (s_wifi_events)
        xEventGroupSetBits(s_wifi_events, PORTAL_SAVED_BIT);

    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void wifi_init(void)
{
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));
}

bool wifi_connect_saved(void)
{
    char ssid[64] = {0}, pass[64] = {0};
    if (!creds_load(ssid, pass)) {
        ESP_LOGI(TAG, "No saved WiFi credentials");
        return false;
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid,     ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(15000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to '%s'", ssid);
        mdns_init();
        mdns_hostname_set(PORTAL_HOSTNAME);
        return true;
    }

    ESP_LOGW(TAG, "Failed to connect to '%s'", ssid);
    esp_wifi_stop();
    return false;
}

void wifi_start_portal(radar_location_t *loc)
{
    ESP_LOGI(TAG, "Starting setup portal AP: %s", PORTAL_AP_NAME);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid_len      = 0,
            .channel       = 1,
            .max_connection = 4,
            .authmode      = WIFI_AUTH_OPEN,
        }
    };
    strlcpy((char *)ap_cfg.ap.ssid, PORTAL_AP_NAME, sizeof(ap_cfg.ap.ssid));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_handle_t server = NULL;
    httpd_config_t server_cfg = HTTPD_DEFAULT_CONFIG();
    server_cfg.server_port = 80;
    ESP_ERROR_CHECK(httpd_start(&server, &server_cfg));

    httpd_uri_t uri_root = { .uri = "/",     .method = HTTP_GET,  .handler = handler_root };
    httpd_uri_t uri_save = { .uri = "/save", .method = HTTP_POST, .handler = handler_save };
    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_save);

    // Also answer any DNS with our IP (captive portal redirect)
    mdns_init();
    mdns_hostname_set(PORTAL_HOSTNAME);

    ESP_LOGI(TAG, "Portal ready — connect to '%s', visit http://" PORTAL_AP_IP,
             PORTAL_AP_NAME);

    // Wait for user to save (or forever if PORTAL_TIMEOUT_S == 0)
    uint32_t wait_ms = PORTAL_TIMEOUT_S ? PORTAL_TIMEOUT_S * 1000 : portMAX_DELAY;
    xEventGroupWaitBits(s_wifi_events, PORTAL_SAVED_BIT,
                        pdFALSE, pdFALSE, pdMS_TO_TICKS(wait_ms));

    httpd_stop(server);
    esp_wifi_stop();

    if (loc) *loc = s_portal_loc;
}

void wifi_erase_credentials(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "WiFi credentials erased");
}

bool wifi_is_connected(void)
{
    if (!s_wifi_events) return false;
    return (xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT) != 0;
}
