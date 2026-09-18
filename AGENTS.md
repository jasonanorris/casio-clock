# AGENTS.md

## Project

This is an embedded firmware project for a standalone, oversized Casio F-91W-inspired digital clock.

Project path:

```text
/home/mint/projects/casio-clock
```

The current milestone is the first F-91W-inspired LVGL clock face on `clock-prototype`. Keep it focused on visual layout and local touch interaction.

## Target Hardware

Target board:

- Waveshare ESP32-S3-Touch-LCD-5B
- 5-inch capacitive touchscreen
- 1024x600 RGB LCD
- ESP32-S3-WROOM-1-N16R8
- 16 MB flash
- 8 MB PSRAM
- Native ESP32-S3 USB usually appears on Linux as `/dev/ttyACM0`, but may re-enumerate as `/dev/ttyACM1` or another ACM number after reset
- USB ID observed by Linux: `303a:1001 Espressif USB JTAG/serial debug unit`

Important warning:

- This is the 1024x600 `ESP32-S3-Touch-LCD-5B` variant.
- Do not substitute settings, pin mappings, LCD timings, or examples for the 800x480 `ESP32-S3-Touch-LCD-5` variant.
- Do not guess LCD timings or GPIO mappings.
- Prefer official Waveshare examples/documentation and Espressif/PlatformIO documentation over assumptions.

## Stack

Preferred stack:

- PlatformIO
- Arduino ESP32 framework initially
- C/C++
- Git
- LVGL 8.4.0 for the current bring-up branch

Do not add unrelated Waveshare libraries until a milestone actually needs them.

## Current PlatformIO Baseline

The first baseline uses a local PlatformIO board manifest because there is no exact standard PlatformIO board ID for this Waveshare board.

Required board-level configuration is kept in `platformio.ini`:

- `board = waveshare_esp32_s3_touch_lcd_5b`
- 16 MB flash
- 8 MB OPI PSRAM
- QIO flash mode
- USB CDC on boot for native USB serial
- upload and monitor ports are passed on the command line instead of fixed in `platformio.ini`

Keep hardware-specific settings isolated in `platformio.ini`, `boards/waveshare_esp32_s3_touch_lcd_5b.json`, and `include/hardware_config.h`.

The `lcd-color-test` branch vendors the official Waveshare-bundled Arduino display libraries under `lib/` for deterministic LCD bring-up:

- `ESP32_Display_Panel` 1.0.0
- `ESP32_IO_Expander` 1.0.1
- `esp-lib-utils` 0.1.2

The current LCD test code lives in `src/lcd_color_test.cpp` and uses the official Waveshare 1024x600 `ESP32-S3-Touch-LCD-5B` timing/pin data plus CH422G LCD reset/backlight pins. Do not replace these with 800x480 settings.

The vendored `ESP32_Display_Panel` driver selection is deliberately configured in `lib/ESP32_Display_Panel/esp_panel_drivers_conf.h` to enable the RGB bus, I2C bus, ST7262 LCD driver, GT911 touch driver, and CH422G IO expander. Without this, the RGB bus factory may log `Disabled or unsupported type: 2(RGB)` and LCD initialization will fail, or the touch driver may be unavailable.

The first visible color-bar test reported about 24 FPS over the refresh callback, matching the current 21 MHz pixel clock and official porch timing. Some visible flicker is expected until display timing/buffering is tuned with verified 1024x600 references.

The touch serial test lives in `src/touch_serial_test.cpp`. It uses official 5B values: GT911 over I2C SDA GPIO 8/SCL GPIO 9 at address `0x5D`, interrupt GPIO 4, and reset through CH422G EXIO1. Hardware has printed touch coordinates and release events successfully.

The LVGL bring-up test lives in `src/lvgl_bringup.cpp` and uses `include/lv_conf.h`. It pins `lvgl/lvgl@8.4.0` in `platformio.ini`, matching Waveshare's LVGL v8 guidance for the ESP32-S3-Touch-LCD-5 family. Hardware displayed the `LVGL OK` screen and the touch button worked.

The `clock-prototype` branch uses `millis()` as its local timebase and can synchronize from NTP over Wi-Fi. It includes the first F-91W-inspired face, a touch-controlled simulated illuminator, and a settings overlay opened by an invisible 200x200 upper-right hotspot. The display defaults to 12-hour time and can be switched between 12-hour and 24-hour formats in Settings. Manual time and format settings are volatile and reset after reboot. Keep this menu extensible for later settings. Do not add RTC or settings persistence unless explicitly asked.

Settings also allows month, day, and weekday adjustment. The temporary calendar advances at midnight using normal month lengths and does not yet handle leap years. Date settings reset after reboot.

Keep UI rendering, settings interactions, and temporary clock state in `src/clock_ui.cpp`. Keep `src/lvgl_bringup.cpp` focused on the verified 1024x600 LCD, GT911 touch, and LVGL driver plumbing.

Build full-screen overlays with `LV_OBJ_FLAG_HIDDEN` set, then reveal them after their child controls and state are complete. This prevents incremental object construction from reaching the display.

LVGL now attempts a full 1024x600 RGB565 draw buffer in PSRAM to avoid banded full-screen transitions, with a 40-row partial-buffer fallback. Preserve the fallback when changing display memory behavior.

Wi-Fi/NTP behavior lives in `src/network_time.cpp`. Credentials live only in the gitignored `include/wifi_credentials.h`; keep `include/wifi_credentials.h.example` free of real credentials. The current timezone is Central Time with US daylight saving rules.

The `rtc-bringup` branch adds the onboard PCF85063ATL using official Waveshare ESP32-S3-Touch-LCD-5 sources. Verified RTC configuration: I2C address `0x51`, SDA GPIO8, SCL GPIO9, shared with touch and CH422G. `src/rtc_time.cpp` uses the already initialized I2C driver; do not call `Wire.begin()` again after touch initialization. Boot priority is valid RTC time first, NTP correction second, and manual/offline time as fallback.

Hardware verification: a powered reset displayed `RTC` before `NTP SYNC`, confirming PCF85063 read-back followed by NTP correction/write-back. Full power-loss retention is not yet tested because no CR927 cell is installed.

The `settings-persistence` branch stores the 12/24-hour preference using the built-in ESP32 `Preferences` NVS API. Manual Save writes year, month, day, weekday, hour, minute, and zero seconds to the RTC. The settings year range is 2020-2069, matching the current PCF85063 encoding used by this project, and calendar rollover handles Gregorian leap years.

Hardware verification: manual RTC values and the 12/24-hour preference survived Reset, followed by successful NTP correction.

## Development Commands

The optional desktop simulator in `simulator/` renders the shared
`src/clock_ui.cpp` at 1024x600 with SDL2. Keep board-independent UI code behind
`src/clock_platform.h`; hardware behavior belongs in `src/clock_platform.cpp`
and desktop behavior in `simulator/clock_platform_sim.cpp`. The simulator does
not validate RGB timing, GT911 touch, RTC, or Wi-Fi behavior.

The LCD clock digits are implemented by the shared `src/seven_segment.*`
component. Preserve the reference layout: large hours/minutes, smaller seconds,
two-dot colon, upper-left `AM`/`PM` or `24H`, centered two-letter weekday, and
SVG-derived day-of-month digits at upper-right.

Digit silhouettes are sourced from
`assets/neat-luulia-densor-8-filled.svg`. Do not hand-edit the generated
`src/seven_segment_assets.*` files; regenerate them with
`python3 tools/generate_segment_assets.py` after changing the source SVG.

Weekday letters use the nine-segment
`assets/neat-luulia-densor-9-filled.svg`, including two center vertical legs
for `M` and `W`. Regenerate `src/weekday_segment_assets.*` with
`python3 tools/generate_weekday_assets.py`; do not hand-edit generated assets.

The full Waveshare 1024x600 screen represents only the watch LCD. Do not add a
simulated watch bezel, case, branding, instructions, source/status labels, or
other text around the clock face. The Settings overlay remains intentionally
separate and is opened through the invisible upper-right hotspot.

Simulator build and run:

```bash
cmake -S simulator -B simulator/build
cmake --build simulator/build --target casio-clock-sim -j
./simulator/build/casio-clock-sim
```

Build:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio run
```

Upload, only after explicit user approval:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio run -t upload --upload-port /dev/ttyACM0
```

Serial monitor:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio device monitor --port /dev/ttyACM0 --baud 115200
```

If the board re-enumerates as `/dev/ttyACM1`, use `/dev/ttyACM1` in the upload or monitor command. Check available ports with `ls -l /dev/ttyACM*`.

## Development Rules

- Inspect the project before modifying it.
- Keep changes small, readable, and maintainable.
- Keep the first milestones extremely simple.
- Ask before flashing the board.
- Ask before making major architecture changes.
- Do not invent hardware configuration.
- Do not copy configuration from the 800x480 model.
- Avoid large dependencies unless they clearly solve the active milestone.
- Update `README.md` and this file when setup, hardware assumptions, or workflow changes.

## Milestones

1. Minimal serial test that prints chip, flash, PSRAM, heap, and heartbeat information. Done.
2. Find and verify the correct official Waveshare 1024x600 example. Done.
3. Display a simple image, color, or test pattern on the LCD. Done.
4. Add touch input. Done.
5. Add LVGL. Done.
6. Build first simple clock screen. Done.
7. Build the clock UI. In progress.
8. Add Wi-Fi/NTP time synchronization. In progress.
9. Evaluate onboard RTC and other peripherals. PCF85063 bring-up verified; CR927 power-loss retention remains untested.
10. Persist clock preferences and manual RTC settings. Done on `settings-persistence`.
