# Contributor Notes

This is an embedded firmware project for a standalone, oversized
Casio F-91W-inspired digital clock.

## Target Hardware

Target board: Waveshare `ESP32-S3-Touch-LCD-5B`.

- ESP32-S3-WROOM-1-N16R8
- 16 MB flash
- 8 MB PSRAM
- 5-inch 1024x600 RGB LCD
- Capacitive touch
- Native ESP32-S3 USB serial/JTAG
- PCF85063ATL RTC

Important: this project targets the 1024x600 `5B` board, not the 800x480
`ESP32-S3-Touch-LCD-5`. Do not substitute LCD timings, GPIO mappings, display
configuration, or examples from the 800x480 model.

## Project Shape

- PlatformIO + Arduino ESP32
- LVGL 8.4.0
- Hardware bring-up: `src/lvgl_bringup.cpp`
- Clock UI, settings, and screensaver: `src/clock_ui.cpp`
- Wi-Fi/NTP: `src/network_time.cpp`
- RTC: `src/rtc_time.cpp`
- User defaults: `include/clock_config.h`
- Local Wi-Fi credentials: `include/wifi_credentials.h`
- Wi-Fi credentials example: `include/wifi_credentials.h.example`

Keep hardware-specific configuration isolated in `platformio.ini`,
`boards/waveshare_esp32_s3_touch_lcd_5b.json`, and
`include/hardware_config.h`.

## Build Commands

Firmware build:

```bash
pio run
```

Upload, using the current native USB serial port:

```bash
pio run -t upload --upload-port /dev/ttyACM0
```

Serial monitor:

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

The board may re-enumerate as `/dev/ttyACM1` or another ACM port after reset.
Check with:

```bash
ls -l /dev/ttyACM*
```

Desktop simulator:

```bash
cmake -S simulator -B simulator/build
cmake --build simulator/build --target casio-clock-sim -j
./simulator/build/casio-clock-sim
```

## Development Rules

- Prefer verified Waveshare 1024x600 `5B` documentation/examples over guesses.
- Do not invent GPIO mappings or LCD timing values.
- Do not commit `include/wifi_credentials.h`; it is intentionally ignored.
- Do not publish firmware binaries built with private Wi-Fi credentials.
- Keep generated segment assets generated. Do not hand-edit
  `src/seven_segment_assets.*` or `src/weekday_segment_assets.*`.
- Regenerate digit assets with the scripts in `tools/` after changing source
  SVGs in `assets/`.
- Keep README setup instructions current when workflow or configuration changes.

## UI Notes

The full 1024x600 display represents only the watch LCD face. Do not add a
simulated watch bezel, branding, instructions, or status text to the main clock
screen.

Invisible hotspots:

- Upper-right 200x200 area opens Settings.
- Upper-left 200x200 area opens the dark bouncing-time screensaver.

Settings currently has `TIME & DATE` and `SCREENSAVER` pages. Saved settings
persist in ESP32 NVS and override compile-time defaults where supported.
