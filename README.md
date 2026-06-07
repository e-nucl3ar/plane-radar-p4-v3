# ESP32-P4 Plane Radar

Open-source ESP-IDF firmware for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C** — a 4″ round 720×720 IPS capacitive-touch display — showing live ADS-B aircraft around your location as a sonar-style radar.

**Based on / inspired by:** [MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar) (ESP32-C3 + 1.28″ GC9A01 SPI display)

---

## Hardware

| Item | Details |
|------|---------|
| **Board** | [Waveshare ESP32-P4-WIFI6-Touch-LCD-4C](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C) |
| **Main SoC** | ESP32-P4 (RISC-V dual-core 360 MHz, 32 MB PSRAM, 32 MB Flash) |
| **WiFi / BT** | ESP32-C6-MINI onboard (Wi-Fi 6, BLE 5 via SDIO / ESP-Hosted) |
| **Display** | 4″ round IPS, 720×720, MIPI-DSI 2-lane, JD9365DA controller |
| **Touch** | GT911 capacitive, I2C (up to 10-point) |

---

## What it does

1. **Wi-Fi setup** (first run) — AP `PlaneRadar-Setup`, captive portal at `http://plane-radar.local` or `http://192.168.4.1`
2. **Radar** — live aircraft from [adsb.fi](https://opendata.adsb.fi/) on a sonar-style green ring grid
3. **Aircraft** — red heading triangle, magenta 5-min speed vector, callsign / altitude tags
4. **Rim dots** — aircraft outside the outer ring shown as red dots at the correct bearing

Compared with the original 240×240 version:
- All geometry scaled ~3× to fill the 720×720 canvas
- Five range presets (5 / 10 / 15 / 25 / **50 km**) — extra preset for the larger screen
- Up to 80 aircraft tracked simultaneously (vs 40)

---

## Controls (BOOT button, GPIO 0)

| Action | Effect |
|--------|--------|
| Short tap | Cycle range preset (5 → 10 → 15 → 25 → 50 km); saved to NVS |
| Hold 3 s | Erase WiFi credentials + location; reboot into setup portal |
| Hold at power-on | Same as long hold |

---

## WiFi setup portal

1. Connect your phone/laptop to **`PlaneRadar-Setup`** (open AP)
2. Open **`http://plane-radar.local`** or **`http://192.168.4.1`**
3. Enter your home WiFi SSID + password, latitude, longitude, and unit preference
4. Click **Save & Connect** — device reboots and connects

Custom fields stored in NVS:

| Field | Purpose |
|-------|---------|
| Latitude / Longitude | Radar center and ADS-B query position |
| Display distances in miles | Ring scale label in **mi** instead of **km** |

---

## Radar display

### Grid
- Deep navy background, subdued green rings and crosshairs
- White **N / S / E / W** at the bezel edge
- Range label on the east spoke at ring 3

### Range presets

| Ring 3 label | Fetch radius |
|---|---|
| 5 km / 3 mi | ~6.7 km |
| 10 km / 6 mi | ~13.3 km (default) |
| 15 km / 9 mi | ~20 km |
| 25 km / 16 mi | ~33.3 km |
| 50 km / 31 mi | ~66.7 km |

### Aircraft
- **Inside outer ring** — red heading triangle, magenta speed vector, callsign / altitude tags
- **Outside ring** — small red dot on rim at correct bearing
- **Tag placement** — towards display center (left/right of symbol based on position)

---

## Project layout

```
CMakeLists.txt
sdkconfig.defaults
partitions.csv
main/
  CMakeLists.txt
  idf_component.yml          ← component manager deps (LVGL, ESP-Hosted, …)
  main.c                     ← app_main, radar loop, button handling
  config.h                   ← all tunable constants & pin definitions
  display.h / display.c      ← MIPI-DSI init via waveshare/esp_lcd_dsi, LVGL task
  radar_theme.h              ← colours & geometry constants (720×720)
  radar_range.h              ← range preset table
  radar_display.h / .c       ← LVGL radar drawing
  radar_location.h / .c      ← NVS persistence for location & prefs
  adsb_client.h / .c         ← HTTP + JSON aircraft fetch
  wifi_setup.h / .c          ← WiFi + captive portal (ESP-IDF httpd)
  status_screens.h / .c      ← LVGL status overlays
.github/workflows/
  build.yml                  ← CI build on push/PR
  release.yml                ← Release asset on git tag
```

---

## Display driver

The 4" round panel is driven by the **`waveshare/esp_lcd_dsi`** IDF component —
a single generic DSI driver Waveshare publishes for all their smaller round and
rectangular DSI panels.  It contains the built-in vendor init sequence and the
correct DPI timing for this panel; **no separate init-command header is needed**.

The component is declared in `main/idf_component.yml` and fetched automatically
on the first build:

```yaml
waveshare/esp_lcd_dsi: ">=1.0.0"
```

Or add it manually:

```sh
idf.py add-dependency "waveshare/esp_lcd_dsi"
```

The DPI timing values used in `display.c` come from the component README
for the *4inch DSI LCD (C)* entry:

| Parameter | Value |
|---|---|
| `dpi_clock_freq_mhz` | 48 |
| `hsync_back_porch` | 32 |
| `hsync_pulse_width` | 200 |
| `hsync_front_porch` | 120 |
| `vsync_back_porch` | 4 |
| `vsync_pulse_width` | 16 |
| `vsync_front_porch` | 8 |

Source: [waveshareteam/Waveshare-ESP32-components](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/master/display/lcd/esp_lcd_dsi)

---

## Build

### Prerequisites
- [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/)
- Target chip: `esp32p4`

### Setup
```sh
git clone https://github.com/YOUR_USERNAME/ESP32-P4-Plane-Radar
cd ESP32-P4-Plane-Radar
idf.py set-target esp32p4
idf.py menuconfig   # optional — review config
idf.py build
```

### Flash
```sh
idf.py -p /dev/ttyUSB0 flash monitor
```

Put the board in download mode by holding **BOOT** then tapping **RESET** (or BOOT at power-on).

### Release binary
Tag a version to trigger the release CI workflow:
```sh
git tag v1.0.0
git push origin v1.0.0
```
The release workflow builds and attaches the three binary files to the GitHub Release.  Flash them with `esptool.py`:
```sh
esptool.py --chip esp32p4 --port /dev/ttyUSB0 write_flash \
  0x0     bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 esp32-p4-plane-radar.bin
```

---

## Configuration

Edit `main/config.h` for hardware and behaviour:

| Area | Keys |
|------|------|
| Display | `DISPLAY_WIDTH`, `DISPLAY_HEIGHT`, `DSI_LANE_NUM`, `DSI_LANE_MBPS` |
| Touch | `TOUCH_I2C_PORT`, `TOUCH_I2C_SDA`, `TOUCH_I2C_SCL` |
| Portal | `PORTAL_AP_NAME`, `PORTAL_AP_IP`, `PORTAL_HOSTNAME` |
| Default location | `DEFAULT_RADAR_LAT`, `DEFAULT_RADAR_LON` |
| ADS-B | `ADSB_FETCH_INTERVAL_MS`, `ADSB_SHOW_GROUND` |
| Button | `BOOT_PIN`, `BOOT_RESET_HOLD_MS` |
| LVGL | `LVGL_TASK_STACK_SIZE`, `LVGL_TASK_CORE` |

Range presets: `main/radar_range.h` (`kRangePresets[]`).  
Colours / geometry: `main/radar_theme.h`.

---

## Key differences from the original (ESP32-C3) project

| | Original (ESP32-C3) | This project (ESP32-P4) |
|--|--|--|
| Framework | Arduino / PlatformIO | ESP-IDF (v5.3+) |
| Display driver | LovyanGFX (SPI) | esp_lcd MIPI-DSI |
| Graphics library | LovyanGFX | LVGL 9 |
| Display | 240×240 GC9A01 | 720×720 JD9365DA round |
| WiFi | Native ESP32-C3 | ESP32-C6 via SDIO / ESP-Hosted |
| Setup portal | WiFiManager (Arduino) | ESP-IDF esp_http_server |
| Aircraft limit | 40 | 80 |
| Range presets | 4 (5/10/15/25 km) | 5 (adds 50 km) |

---

## Dependencies

- [ESP-IDF](https://github.com/espressif/esp-idf) ≥ 5.3
- [LVGL](https://github.com/lvgl/lvgl) ≥ 9.2 (via idf-component-manager)
- [ESP-Hosted](https://github.com/espressif/esp-hosted) ≥ 0.0.6 (SDIO WiFi transport)
- cJSON (bundled with ESP-IDF)
- mdns (bundled with ESP-IDF)

---

## ADS-B data

Source: `https://opendata.adsb.fi/api/v2/` — free, no API key required.  
The fetch radius scales automatically with the active range preset.

---

## License

MIT — see [LICENSE](LICENSE).  
Original project © MatixYo — used under MIT License.
