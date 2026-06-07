// =============================================================================
// status_screens.c — Full-screen status overlays using LVGL
// =============================================================================

#include "status_screens.h"
#include "radar_theme.h"
#include "display.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "screens";

// Shared screen object — one at a time
static lv_obj_t *s_screen = NULL;

static lv_obj_t *make_screen(lv_color_t bg)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(scr, bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    return scr;
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text,
                            lv_color_t color, const lv_font_t *font,
                            lv_align_t align, lv_coord_t y_ofs)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_align(lbl, align, 0, y_ofs);
    return lbl;
}

void screen_connecting(const char *ssid)
{
    display_lock();
    if (s_screen) { lv_obj_del(s_screen); s_screen = NULL; }

    s_screen = make_screen(STATUS_BG_COLOR);

    char buf[80];
    add_label(s_screen, LV_SYMBOL_WIFI " Connecting…",
              STATUS_WARN_COLOR, STATUS_FONT, LV_ALIGN_CENTER, -40);

    snprintf(buf, sizeof(buf), "SSID: %s", ssid ? ssid : "…");
    add_label(s_screen, buf, STATUS_TEXT_COLOR, STATUS_FONT_SMALL,
              LV_ALIGN_CENTER, 20);

    lv_scr_load(s_screen);
    display_unlock();
}

void screen_portal(void)
{
    display_lock();
    if (s_screen) { lv_obj_del(s_screen); s_screen = NULL; }

    s_screen = make_screen(lv_color_make(0x15, 0x15, 0x00));

    add_label(s_screen, LV_SYMBOL_SETTINGS " Setup Portal",
              STATUS_WARN_COLOR, STATUS_FONT, LV_ALIGN_CENTER, -80);

    add_label(s_screen, "Connect to Wi-Fi:",
              STATUS_TEXT_COLOR, STATUS_FONT_SMALL, LV_ALIGN_CENTER, -20);
    add_label(s_screen, PORTAL_AP_NAME,
              STATUS_WARN_COLOR, STATUS_FONT, LV_ALIGN_CENTER, 30);
    add_label(s_screen, "Then open:",
              STATUS_TEXT_COLOR, STATUS_FONT_SMALL, LV_ALIGN_CENTER, 90);
    add_label(s_screen, "http://plane-radar.local",
              STATUS_WARN_COLOR, STATUS_FONT_SMALL, LV_ALIGN_CENTER, 130);
    add_label(s_screen, "or  http://" PORTAL_AP_IP,
              STATUS_TEXT_COLOR, STATUS_FONT_SMALL, LV_ALIGN_CENTER, 165);

    lv_scr_load(s_screen);
    display_unlock();
}

void screen_message(const char *title, const char *body, uint32_t dismiss_ms)
{
    display_lock();
    if (s_screen) { lv_obj_del(s_screen); s_screen = NULL; }

    s_screen = make_screen(STATUS_BG_COLOR);
    add_label(s_screen, title, STATUS_WARN_COLOR, STATUS_FONT,
              LV_ALIGN_CENTER, -30);
    if (body)
        add_label(s_screen, body, STATUS_TEXT_COLOR, STATUS_FONT_SMALL,
                  LV_ALIGN_CENTER, 40);
    lv_scr_load(s_screen);
    display_unlock();

    if (dismiss_ms) vTaskDelay(pdMS_TO_TICKS(dismiss_ms));
}

void screen_clear(void)
{
    display_lock();
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
    display_unlock();
}
