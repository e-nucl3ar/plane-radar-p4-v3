// =============================================================================
// wifi_setup.h — WiFi credential storage and captive-portal setup
//
// On this hardware, WiFi is provided by the onboard ESP32-C6-MINI module
// via SDIO / ESP-Hosted.  The P4 side uses standard ESP-IDF WiFi APIs just
// as if it had native WiFi — the hosted driver is transparent.
//
// Portal logic is a lightweight HTTP server (not WiFiManager which is
// Arduino-specific).  The user visits http://plane-radar.local to configure:
//   • Home WiFi SSID + password
//   • Radar lat / lon
//   • km vs miles preference
// =============================================================================
#pragma once
#include <stdbool.h>
#include "radar_location.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the WiFi subsystem (ESP-Hosted + esp_netif).
 *        Must be called after nvs_flash_init().
 */
void wifi_init(void);

/**
 * @brief Try to connect to the saved SSID.
 * @return true if connected within timeout.
 */
bool wifi_connect_saved(void);

/**
 * @brief Start the captive-portal AP and HTTP setup server.
 *        Blocks until the user saves credentials (or portal_timeout_s elapses).
 *        Populates *loc with any user-entered location.
 */
void wifi_start_portal(radar_location_t *loc);

/**
 * @brief Erase saved WiFi credentials from NVS.
 */
void wifi_erase_credentials(void);

/**
 * @return true if the device currently has an IP address.
 */
bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif
