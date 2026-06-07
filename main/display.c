// =============================================================================
// display.c — MIPI-DSI display + LVGL initialisation
//
// Hardware: Waveshare ESP32-P4-WIFI6-Touch-LCD-4C
//   Panel:  4" round IPS 720×720, driven via waveshare/esp_lcd_dsi component
//
// Driver pipeline:
//   1. Acquire LDO channel 3 (2500 mV) to power the MIPI DSI PHY
//   2. Create 2-lane MIPI DSI bus
//   3. Create DBI (command) I/O for panel init
//   4. Create DPI panel via waveshare esp_lcd_dsi component
//   5. Init LVGL with two PSRAM draw buffers, start tick timer + handler task
//
// Timing for 4inch DSI LCD (C) — 720×720 @ ~60 Hz
//   dpi_clock_freq_mhz : 48
//   hsync_back_porch   : 32     vsync_back_porch  : 4
//   hsync_pulse_width  : 200    vsync_pulse_width : 16
//   hsync_front_porch  : 120    vsync_front_porch : 8
//
// Source: waveshareteam/Waveshare-ESP32-components display/lcd/esp_lcd_dsi
// =============================================================================

#include "display.h"
#include "config.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_dpi.h"   // esp_lcd_new_panel_dpi()
#include "esp_ldo_regulator.h"   // esp_ldo_acquire_channel()
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static const char *TAG = "display";

// ---------------------------------------------------------------------------
// DPI timing for 4-inch DSI LCD (C) — 720×720 round panel
// Source: waveshareteam/Waveshare-ESP32-components esp_lcd_dsi README
// ---------------------------------------------------------------------------
#define PANEL_DPI_CLK_MHZ       48
#define PANEL_HSYNC_BACK_PORCH  32
#define PANEL_HSYNC_PULSE_WIDTH 200
#define PANEL_HSYNC_FRONT_PORCH 120
#define PANEL_VSYNC_BACK_PORCH  4
#define PANEL_VSYNC_PULSE_WIDTH 16
#define PANEL_VSYNC_FRONT_PORCH 8

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static SemaphoreHandle_t        s_lvgl_mux   = NULL;
static esp_lcd_panel_handle_t   s_panel      = NULL;
static lv_disp_t               *s_disp       = NULL;
static esp_ldo_channel_handle_t s_ldo_mipi   = NULL;

// ---------------------------------------------------------------------------
// LVGL flush callback
// ---------------------------------------------------------------------------
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    int x1 = area->x1, y1 = area->y1;
    int x2 = area->x2 + 1, y2 = area->y2 + 1;
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, color_map);
    lv_disp_flush_ready(drv);
}

// ---------------------------------------------------------------------------
// LVGL tick timer callback
// ---------------------------------------------------------------------------
static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// ---------------------------------------------------------------------------
// LVGL handler task — pinned to core 1
// ---------------------------------------------------------------------------
static void lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL handler task running on core %d", xPortGetCoreID());
    for (;;) {
        xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY);
        uint32_t ms = lv_timer_handler();
        xSemaphoreGiveRecursive(s_lvgl_mux);
        vTaskDelay(pdMS_TO_TICKS(ms < 5 ? 5 : ms));
    }
}

// ---------------------------------------------------------------------------
// Public API — display_init
// ---------------------------------------------------------------------------
esp_err_t display_init(void)
{
    // ---- 1. Power on MIPI DSI PHY via internal LDO regulator ---------------
    // All Waveshare P4 DSI boards use LDO channel 3 at 2500 mV.
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = 3,
        .voltage_mv = 2500,
    };
    ESP_RETURN_ON_ERROR(
        esp_ldo_acquire_channel(&ldo_cfg, &s_ldo_mipi),
        TAG, "LDO channel 3 (MIPI PHY) acquire failed");
    ESP_LOGI(TAG, "MIPI DSI PHY LDO powered on (2500 mV)");

    // ---- 2. Create 2-lane MIPI DSI bus --------------------------------------
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t dsi_bus_cfg = {
        .bus_id              = 0,
        .num_data_lanes      = DSI_LANE_NUM,
        .phy_clk_src         = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps  = DSI_LANE_MBPS,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_dsi_bus(&dsi_bus_cfg, &dsi_bus),
        TAG, "DSI bus creation failed");
    ESP_LOGI(TAG, "MIPI DSI bus created (%d lanes @ %d Mbps)",
             DSI_LANE_NUM, DSI_LANE_MBPS);

    // ---- 3. Create DBI (command-mode) I/O for panel init sequence ----------
    esp_lcd_panel_io_handle_t dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &dbi_io),
        TAG, "DBI IO creation failed");

    // ---- 4. DPI (video-mode) timing config ----------------------------------
    //
    // These values come directly from the waveshare/esp_lcd_dsi component README
    // for the "4inch DSI LCD (C)" — the 720×720 round panel on this board.
    //
    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel     = 0,
        .dpi_clk_src         = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz  = PANEL_DPI_CLK_MHZ,
        .pixel_format        = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .video_timing = {
            .h_size            = DISPLAY_WIDTH,
            .v_size            = DISPLAY_HEIGHT,
            .hsync_back_porch  = PANEL_HSYNC_BACK_PORCH,
            .hsync_pulse_width = PANEL_HSYNC_PULSE_WIDTH,
            .hsync_front_porch = PANEL_HSYNC_FRONT_PORCH,
            .vsync_back_porch  = PANEL_VSYNC_BACK_PORCH,
            .vsync_pulse_width = PANEL_VSYNC_PULSE_WIDTH,
            .vsync_front_porch = PANEL_VSYNC_FRONT_PORCH,
        },
        .flags.use_dma2d     = true,  // use 2D-DMA for accelerated blitting
    };

    // ---- 5. Create DPI panel via waveshare/esp_lcd_dsi component ------------
    //
    // esp_lcd_new_panel_dpi() is provided by the waveshare/esp_lcd_dsi
    // component (idf_component.yml dependency "waveshare/esp_lcd_dsi").
    // It sends the built-in vendor init sequence for the panel automatically.
    //
    esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num  = GPIO_NUM_NC,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel  = 16,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_dpi(dsi_bus, &dbi_io, &dpi_cfg, &panel_dev_cfg, &s_panel),
        TAG, "DPI panel creation failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),     TAG, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "Panel on failed");
    ESP_LOGI(TAG, "Panel online: %d×%d @ %d MHz DPI",
             DISPLAY_WIDTH, DISPLAY_HEIGHT, PANEL_DPI_CLK_MHZ);

    // ---- 6. Optional backlight (LEDC PWM) -----------------------------------
    if (LCD_BL_PIN != GPIO_NUM_NC) {
        ledc_timer_config_t bl_tmr = {
            .speed_mode      = LEDC_LOW_SPEED_MODE,
            .timer_num       = LEDC_TIMER_0,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .freq_hz         = LCD_BL_FREQ_HZ,
            .clk_cfg         = LEDC_AUTO_CLK,
        };
        ledc_channel_config_t bl_ch = {
            .gpio_num   = LCD_BL_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = LCD_BL_LEDC_CH,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 255,
            .hpoint     = 0,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&bl_tmr));
        ESP_ERROR_CHECK(ledc_channel_config(&bl_ch));
    }

    // ---- 7. LVGL init -------------------------------------------------------
    lv_init();

    // Allocate two draw buffers in PSRAM (1/10 screen height each)
    const size_t buf_px   = DISPLAY_WIDTH * (DISPLAY_HEIGHT / 10);
    lv_color_t  *buf1 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t  *buf2 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers in PSRAM");
        return ESP_ERR_NO_MEM;
    }

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_px);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = DISPLAY_WIDTH;
    disp_drv.ver_res   = DISPLAY_HEIGHT;
    disp_drv.flush_cb  = lvgl_flush_cb;
    disp_drv.draw_buf  = &draw_buf;
    disp_drv.user_data = s_panel;
    s_disp = lv_disp_drv_register(&disp_drv);

    // ---- 8. LVGL tick timer -------------------------------------------------
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG,
                        "LVGL tick timer create failed");
    ESP_RETURN_ON_ERROR(
        esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000ULL),
        TAG, "LVGL tick timer start failed");

    // ---- 9. LVGL mutex + handler task ---------------------------------------
    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    configASSERT(s_lvgl_mux);
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL,
                            LVGL_TASK_PRIORITY, NULL, LVGL_TASK_CORE);

    ESP_LOGI(TAG, "Display fully initialised");
    return ESP_OK;
}

void display_lock(void)
{
    xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY);
}

void display_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}

lv_disp_t *display_get(void)
{
    return s_disp;
}

void display_set_backlight(uint8_t pct)
{
    if (LCD_BL_PIN == GPIO_NUM_NC) return;
    uint32_t duty = ((uint32_t)pct * 255u) / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CH, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CH);
}
