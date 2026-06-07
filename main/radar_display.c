// =============================================================================
// radar_display.c — ADS-B radar rendered with LVGL on the 720×720 round screen
//
// Ported from MatixYo/ESP32-Plane-Radar.
//   • Original: LovyanGFX + GC9A01 240×240 SPI
//   • This port: LVGL + JD9365DA 720×720 MIPI-DSI
//
// All pixel coordinates are scaled by ~3× from the original.
// Uses LVGL canvas (lv_canvas) for per-pixel drawing; the canvas buffer lives
// in PSRAM and is flushed to the DSI framebuffer by the LVGL flush callback.
// =============================================================================

#include "radar_display.h"
#include "radar_theme.h"
#include "radar_range.h"
#include "config.h"
#include "display.h"
#include "esp_log.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "radar";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------
static lv_obj_t    *s_canvas  = NULL;
static lv_color_t  *s_cbuf   = NULL;  // PSRAM draw buffer

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
#define DEG2RAD(d)  ((d) * (float)(M_PI / 180.0))

// Convert bearing (degrees, N=0, CW) and distance (km) to canvas X/Y
static void bearing_to_xy(float bearing_deg, float dist_km, float range_outer_km,
                           int *out_x, int *out_y)
{
    float r = (dist_km / range_outer_km) * RADAR_OUTER_RADIUS;
    float angle = DEG2RAD(bearing_deg - 90.0f); // N up → canvas: 0° = right
    *out_x = (int)(RADAR_CENTER_X + r * cosf(angle));
    *out_y = (int)(RADAR_CENTER_Y + r * sinf(angle));
}

// Haversine distance (km) between two lat/lon points
static float haversine_km(float lat1, float lon1, float lat2, float lon2)
{
    float dlat = DEG2RAD(lat2 - lat1);
    float dlon = DEG2RAD(lon2 - lon1);
    float a = sinf(dlat/2)*sinf(dlat/2) +
              cosf(DEG2RAD(lat1))*cosf(DEG2RAD(lat2)) *
              sinf(dlon/2)*sinf(dlon/2);
    return 6371.0f * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

// Bearing (deg, N=0 CW) from point 1 to point 2
static float bearing_to(float lat1, float lon1, float lat2, float lon2)
{
    float dlon = DEG2RAD(lon2 - lon1);
    float y = sinf(dlon) * cosf(DEG2RAD(lat2));
    float x = cosf(DEG2RAD(lat1))*sinf(DEG2RAD(lat2)) -
              sinf(DEG2RAD(lat1))*cosf(DEG2RAD(lat2))*cosf(dlon);
    float b = atan2f(y, x) * 180.0f / (float)M_PI;
    return fmodf(b + 360.0f, 360.0f);
}

// ---------------------------------------------------------------------------
// Drawing helpers (LVGL draw_ctx on canvas)
// ---------------------------------------------------------------------------

static lv_draw_ctx_t *s_draw_ctx = NULL;

static void draw_line_px(int x0, int y0, int x1, int y1,
                          lv_color_t color, uint8_t width)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.opa   = LV_OPA_COVER;
    lv_point_t p1 = {x0, y0}, p2 = {x1, y1};
    lv_canvas_draw_line(s_canvas, &p1, &p2, 1, &dsc);
}

static void draw_circle_outline(int cx, int cy, int r,
                                  lv_color_t color, uint8_t width)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color     = color;
    dsc.width     = width;
    dsc.opa       = LV_OPA_COVER;
    dsc.img_src   = NULL;
    lv_canvas_draw_arc(s_canvas, cx, cy, r, 0, 360, &dsc);
}

static void draw_filled_circle(int cx, int cy, int r, lv_color_t color)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color  = color;
    dsc.bg_opa    = LV_OPA_COVER;
    dsc.radius    = LV_RADIUS_CIRCLE;
    dsc.border_width = 0;
    lv_area_t area = {cx-r, cy-r, cx+r, cy+r};
    lv_canvas_draw_rect(s_canvas, area.x1, area.y1,
                        area.x2-area.x1+1, area.y2-area.y1+1, &dsc);
}

static void draw_text(int x, int y, const char *text,
                       lv_color_t color, const lv_font_t *font,
                       lv_text_align_t align)
{
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font  = font;
    dsc.align = align;
    dsc.opa   = LV_OPA_COVER;
    lv_area_t area = {x - 120, y - 20, x + 120, y + 40};
    lv_canvas_draw_text(s_canvas, area.x1, area.y1,
                        area.x2 - area.x1, &dsc, text);
}

// Draw a small filled equilateral triangle (heading indicator)
static void draw_aircraft_triangle(int cx, int cy, float heading_deg,
                                    int size, lv_color_t color)
{
    float h = DEG2RAD(heading_deg - 90.0f);
    // Tip and two base corners
    lv_point_t pts[4];
    pts[0].x = (int16_t)(cx + size * cosf(h));
    pts[0].y = (int16_t)(cy + size * sinf(h));
    pts[1].x = (int16_t)(cx + size * 0.6f * cosf(h + (float)(M_PI * 0.75)));
    pts[1].y = (int16_t)(cy + size * 0.6f * sinf(h + (float)(M_PI * 0.75)));
    pts[2].x = (int16_t)(cx + size * 0.6f * cosf(h - (float)(M_PI * 0.75)));
    pts[2].y = (int16_t)(cy + size * 0.6f * sinf(h - (float)(M_PI * 0.75)));
    pts[3] = pts[0];

    lv_draw_polygon_dsc_t dsc;
    lv_draw_polygon_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa   = LV_OPA_COVER;
    lv_canvas_draw_polygon(s_canvas, pts, 3, &dsc);
}

// ---------------------------------------------------------------------------
// Grid background (rings + crosshairs + compass labels)
// ---------------------------------------------------------------------------
static void draw_grid(const radar_location_t *loc)
{
    // Fill background
    lv_canvas_fill_bg(s_canvas, RADAR_BG_COLOR, LV_OPA_COVER);

    // Clip circle mask (draw a black rect outside the round screen boundary)
    // On a round display the hardware clips; nothing extra needed in software.

    // Concentric rings
    for (int i = 1; i <= RADAR_RING_COUNT; i++) {
        int r = (RADAR_OUTER_RADIUS * i) / RADAR_RING_COUNT;
        draw_circle_outline(RADAR_CENTER_X, RADAR_CENTER_Y, r,
                            RADAR_RING_COLOR, RADAR_RING_STROKE);
    }

    // Crosshairs
    draw_line_px(RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_OUTER_RADIUS,
                 RADAR_CENTER_X, RADAR_CENTER_Y + RADAR_OUTER_RADIUS,
                 RADAR_CROSS_COLOR, RADAR_CROSS_STROKE);
    draw_line_px(RADAR_CENTER_X - RADAR_OUTER_RADIUS, RADAR_CENTER_Y,
                 RADAR_CENTER_X + RADAR_OUTER_RADIUS, RADAR_CENTER_Y,
                 RADAR_CROSS_COLOR, RADAR_CROSS_STROKE);

    // Compass labels
    draw_text(RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_OUTER_RADIUS - 5,
              "N", RADAR_LABEL_COLOR, RADAR_COMPASS_FONT, LV_TEXT_ALIGN_CENTER);
    draw_text(RADAR_CENTER_X, RADAR_CENTER_Y + RADAR_OUTER_RADIUS - 20,
              "S", RADAR_LABEL_COLOR, RADAR_COMPASS_FONT, LV_TEXT_ALIGN_CENTER);
    draw_text(RADAR_CENTER_X - RADAR_OUTER_RADIUS - 5,
              RADAR_CENTER_Y - 14,
              "W", RADAR_LABEL_COLOR, RADAR_COMPASS_FONT, LV_TEXT_ALIGN_RIGHT);
    draw_text(RADAR_CENTER_X + RADAR_OUTER_RADIUS - 30,
              RADAR_CENTER_Y - 14,
              "E", RADAR_LABEL_COLOR, RADAR_COMPASS_FONT, LV_TEXT_ALIGN_LEFT);

    // Range label on east spoke at ring 3
    int ring3_r = (RADAR_OUTER_RADIUS * 3) / RADAR_RING_COUNT;
    char range_buf[16];
    uint8_t ridx = loc->range_idx;
    if (ridx >= RANGE_PRESET_COUNT) ridx = 1;
    snprintf(range_buf, sizeof(range_buf), "%s",
             loc->use_miles ? kRangePresets[ridx].label_mi
                            : kRangePresets[ridx].label_km);
    draw_text(RADAR_CENTER_X + ring3_r + 4, RADAR_CENTER_Y - 18,
              range_buf, RADAR_RANGE_COLOR, RADAR_RANGE_FONT, LV_TEXT_ALIGN_LEFT);

    // Center dot
    draw_filled_circle(RADAR_CENTER_X, RADAR_CENTER_Y,
                       RADAR_CENTER_DOT_R, RADAR_CENTER_COLOR);
}

// ---------------------------------------------------------------------------
// Aircraft rendering
// ---------------------------------------------------------------------------
static void draw_aircraft(const aircraft_t *ac, const radar_location_t *loc)
{
    float outer_km = radar_range_fetch_km(loc->range_idx);

    float dist_km  = haversine_km(loc->lat, loc->lon, ac->lat, ac->lon);
    float bearing  = bearing_to(loc->lat, loc->lon, ac->lat, ac->lon);

    int ax, ay;
    bearing_to_xy(bearing, dist_km, outer_km, &ax, &ay);

    bool inside = (dist_km <= outer_km);

    if (!inside) {
        // Aircraft beyond outer ring — place a dot on the rim at correct bearing
        float angle = DEG2RAD(bearing - 90.0f);
        int rx = (int)(RADAR_CENTER_X + (RADAR_OUTER_RADIUS - RIM_DOT_R - 2) * cosf(angle));
        int ry = (int)(RADAR_CENTER_Y + (RADAR_OUTER_RADIUS - RIM_DOT_R - 2) * sinf(angle));
        draw_filled_circle(rx, ry, RIM_DOT_R, RIM_DOT_COLOR);
        return;
    }

    // Aircraft inside ring — draw heading triangle
    draw_aircraft_triangle(ax, ay, ac->heading_deg,
                            AIRCRAFT_TRIANGLE_SIZE, AIRCRAFT_ICON_COLOR);

    // Speed vector (clipped at outer ring boundary)
    if (ac->speed_kts > 20.0f) {
        float vec_len_km = (ac->speed_kts / 1852.0f) * 5.0f; // 5-min projection
        float vx_km = vec_len_km * sinf(DEG2RAD(ac->heading_deg));
        float vy_km = -vec_len_km * cosf(DEG2RAD(ac->heading_deg));
        float scale = (float)RADAR_OUTER_RADIUS / outer_km;
        int vx2 = ax + (int)(vx_km * scale);
        int vy2 = ay + (int)(vy_km * scale);
        // Clamp vector endpoint to outer circle
        float dx = vx2 - RADAR_CENTER_X, dy = vy2 - RADAR_CENTER_Y;
        float mag = sqrtf(dx*dx + dy*dy);
        if (mag > RADAR_OUTER_RADIUS) {
            vx2 = (int)(RADAR_CENTER_X + dx * RADAR_OUTER_RADIUS / mag);
            vy2 = (int)(RADAR_CENTER_Y + dy * RADAR_OUTER_RADIUS / mag);
        }
        draw_line_px(ax, ay, vx2, vy2, AIRCRAFT_VECTOR_COLOR, 2);
    }

    // Tag placement: west of center → tag to the right; east → tag to the left
    int tag_x_off = (ax < RADAR_CENTER_X) ? 20 : -20;
    lv_text_align_t tag_align = (ax < RADAR_CENTER_X) ? LV_TEXT_ALIGN_LEFT
                                                       : LV_TEXT_ALIGN_RIGHT;

    // Line 1: callsign / type
    char line1[20];
    if (ac->callsign[0])
        snprintf(line1, sizeof(line1), "%s", ac->callsign);
    else if (ac->type[0])
        snprintf(line1, sizeof(line1), "%s", ac->type);
    else
        snprintf(line1, sizeof(line1), "?");

    draw_text(ax + tag_x_off, ay - 22, line1,
              AIRCRAFT_TAG_COLOR, RADAR_TAG_FONT, tag_align);

    // Line 2: altitude  (FL or ft)
    if (ac->alt_ft != 0) {
        char alt_buf[12];
        if (ac->alt_ft >= 1000)
            snprintf(alt_buf, sizeof(alt_buf), "FL%03d", ac->alt_ft / 100);
        else
            snprintf(alt_buf, sizeof(alt_buf), "%dft", ac->alt_ft);
        draw_text(ax + tag_x_off, ay + 2, alt_buf,
                  AIRCRAFT_TAG_COLOR, RADAR_TAG_FONT, tag_align);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void radar_display_init(void)
{
    size_t buf_size = DISPLAY_WIDTH * DISPLAY_HEIGHT;

    s_cbuf = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t),
                                             MALLOC_CAP_SPIRAM);
    if (!s_cbuf) {
        ESP_LOGE(TAG, "Cannot allocate canvas buffer in PSRAM");
        return;
    }

    s_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(s_canvas, s_cbuf, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(s_canvas, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "Radar canvas created (%d×%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void radar_display_show(void)
{
    display_lock();
    if (s_canvas) lv_scr_load(lv_obj_get_parent(s_canvas));
    display_unlock();
}

void radar_display_update(const aircraft_t *aircraft, int count,
                           const radar_location_t *loc)
{
    if (!s_canvas) return;

    draw_grid(loc);

    int drawn = 0;
    for (int i = 0; i < count; i++) {
        if (!aircraft[i].valid) continue;
        draw_aircraft(&aircraft[i], loc);
        drawn++;
    }
    ESP_LOGD(TAG, "Rendered %d aircraft", drawn);

    // Trigger LVGL to flush the canvas to the display
    lv_obj_invalidate(s_canvas);
}
