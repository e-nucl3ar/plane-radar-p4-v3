// =============================================================================
// status_screens.h — Splash, connecting, and setup-portal status displays
// =============================================================================
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Show a "Connecting…" screen with optional sub-text. */
void screen_connecting(const char *ssid);

/** Show the setup portal screen with AP name + IP. */
void screen_portal(void);

/** Show a temporary message (auto-dismiss after ms). */
void screen_message(const char *title, const char *body, uint32_t dismiss_ms);

/** Clear any status screen (return to radar screen). */
void screen_clear(void);

#ifdef __cplusplus
}
#endif
