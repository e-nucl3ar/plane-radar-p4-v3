// =============================================================================
// main.c — ESP32-P4 Plane Radar
//
// Application flow (mirrors the original ESP32-C3 project):
//   1. Init NVS, display, WiFi
//   2. Check BOOT button: long-press → factory reset
//   3. Try to connect with saved credentials
//   4. If no credentials / failed → open setup portal, wait
//   5. Radar loop: fetch ADS-B every 5 s, redraw display
//   6. BOOT short-tap → cycle range preset
// =============================================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "config.h"
#include "display.h"
#include "radar_display.h"
#include "radar_location.h"
#include "radar_range.h"
#include "adsb_client.h"
#include "wifi_setup.h"
#include "status_screens.h"

static const char *TAG = "main";

// ---------------------------------------------------------------------------
// BOOT button handling
// ---------------------------------------------------------------------------

static bool boot_held_at_startup(void)
{
    gpio_set_direction(BOOT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_PIN, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50));
    return (gpio_get_level(BOOT_PIN) == 0);
}

static bool boot_wait_for_long_press(void)
{
    // Already detected LOW at startup — wait to see if it's a long press
    uint32_t held_ms = 0;
    while (gpio_get_level(BOOT_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
        held_ms += 50;
        if (held_ms >= BOOT_RESET_HOLD_MS) return true;
    }
    return false;
}

// Short tap (non-blocking): check if button was briefly pressed
static bool boot_was_tapped(void)
{
    if (gpio_get_level(BOOT_PIN) != 0) return false;
    vTaskDelay(pdMS_TO_TICKS(BOOT_TAP_MIN_MS));
    if (gpio_get_level(BOOT_PIN) != 0) {
        // Released — it was a tap
        return true;
    }
    // Still held — absorb but don't count as tap (long press handled elsewhere)
    while (gpio_get_level(BOOT_PIN) == 0)
        vTaskDelay(pdMS_TO_TICKS(50));
    return false;
}

// ---------------------------------------------------------------------------
// app_main
// ---------------------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-P4 Plane Radar starting");

    // NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Display + LVGL
    ESP_ERROR_CHECK(display_init());
    display_set_backlight(100);

    // Splash
    screen_message("✈  Plane Radar", "Starting…", 800);

    // Boot button check
    bool held = boot_held_at_startup();
    if (held && boot_wait_for_long_press()) {
        screen_message("Factory Reset", "Clearing WiFi & location…", 1500);
        wifi_erase_credentials();
        radar_location_erase();
        screen_message("Done", "Rebooting…", 1000);
        esp_restart();
    }

    // Load saved location / prefs
    radar_location_t loc;
    radar_location_load(&loc);

    // WiFi init (ESP-Hosted SDIO transport initialised inside)
    wifi_init();

    // Try to connect
    bool connected = false;
    {
        char ssid_buf[64] = {0};
        // Peek at saved SSID for the screen label
        nvs_handle_t h;
        if (nvs_open("wifi_creds", NVS_READONLY, &h) == ESP_OK) {
            size_t n = sizeof(ssid_buf);
            nvs_get_str(h, "ssid", ssid_buf, &n);
            nvs_close(h);
        }
        screen_connecting(ssid_buf[0] ? ssid_buf : NULL);
        connected = wifi_connect_saved();
    }

    if (!connected) {
        screen_portal();
        wifi_start_portal(&loc);
        radar_location_save(&loc);
        // Reboot so we connect cleanly with the new credentials
        screen_message("Saved!", "Rebooting…", 1000);
        esp_restart();
    }

    // Build radar canvas
    radar_display_init();
    radar_display_show();

    // ---- Main radar loop --------------------------------------------------
    aircraft_t *aircraft = heap_caps_malloc(
        ADSB_MAX_AIRCRAFT * sizeof(aircraft_t), MALLOC_CAP_SPIRAM);
    if (!aircraft) {
        ESP_LOGE(TAG, "No memory for aircraft array");
        screen_message("ERROR", "No memory", 0);
        return;
    }

    TickType_t last_fetch = 0;

    for (;;) {
        TickType_t now = xTaskGetTickCount();

        // Range cycle on short tap
        if (boot_was_tapped()) {
            loc.range_idx = (loc.range_idx + 1) % RANGE_PRESET_COUNT;
            radar_location_save(&loc);
            ESP_LOGI(TAG, "Range preset → %d (%s)",
                     loc.range_idx, kRangePresets[loc.range_idx].label_km);
        }

        // Long press → factory reset
        if (gpio_get_level(BOOT_PIN) == 0) {
            if (boot_wait_for_long_press()) {
                screen_message("Factory Reset", "Clearing WiFi & location…", 1500);
                wifi_erase_credentials();
                radar_location_erase();
                esp_restart();
            }
        }

        // ADS-B fetch
        if ((now - last_fetch) >= pdMS_TO_TICKS(ADSB_FETCH_INTERVAL_MS)) {
            last_fetch = now;
            float radius = radar_range_fetch_km(loc.range_idx);
            int count = adsb_fetch(loc.lat, loc.lon, radius,
                                   aircraft, ADSB_MAX_AIRCRAFT);

            display_lock();
            radar_display_update(aircraft, count, &loc);
            display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
