# casio-clock

Embedded firmware project for a standalone, oversized Casio F-91W-inspired digital clock.

## Target Hardware

This project targets the Waveshare `ESP32-S3-Touch-LCD-5B` development board:

- ESP32-S3-WROOM-1-N16R8 module
- 16 MB flash
- 8 MB PSRAM
- 5-inch 1024x600 RGB LCD
- Capacitive touch
- Native ESP32-S3 USB
- Linux serial device: usually `/dev/ttyACM0`, but native USB can re-enumerate as `/dev/ttyACM1` or another ACM number after reset
- Observed USB ID: `303a:1001 Espressif USB JTAG/serial debug unit`

Important: this is the 1024x600 `5B` model, not the 800x480 `ESP32-S3-Touch-LCD-5` model. LCD settings, pin mappings, timings, and examples must be verified for the 1024x600 board before use.

## Development Environment

- Linux Mint
- VS Code
- Codex
- PlatformIO CLI
- Arduino ESP32 framework
- C/C++
- Git

PlatformIO is installed locally for this project in `.venv`. Use `.venv/bin/pio` from the project root.

## Current Project Status

Current milestone: first F-91W-inspired LVGL clock face on `clock-prototype`.

The firmware:

- initializes serial at 115200 baud
- prints a clear startup banner
- prints ESP32 chip information
- prints flash size
- prints detected PSRAM size
- prints heap and PSRAM availability
- initializes the CH422G IO expander for LCD reset/backlight
- initializes the ST7262 RGB LCD using the official Waveshare 1024x600 `ESP32-S3-Touch-LCD-5B` timing/pin data
- initializes LVGL 8.4.0
- displays an F-91W-inspired watch face with digital `HH:MM:SS`, weekday, and date indicators
- initializes the GT911 capacitive touch controller using the official Waveshare 1024x600 `ESP32-S3-Touch-LCD-5B` touch data
- routes touch input into LVGL and toggles the simulated LCD illuminator when pressed
- opens a settings menu from an invisible 200x200 hotspot in the upper-right corner
- allows the temporary local clock time to be adjusted by hour and minute
- supports 12-hour and 24-hour display formats, defaulting to 12-hour
- allows month, day, and weekday to be adjusted in Settings
- prints a heartbeat once per second

The firmware can connect to Wi-Fi and synchronize from NTP when local credentials are configured. It uses the `America/Chicago` Central Time rules, including daylight saving time. If Wi-Fi or NTP is unavailable, the displayed time and calendar continue locally using `millis()` and the manual settings remain available. Manual settings are not yet persisted across reboot.

The `rtc-bringup` branch adds the onboard PCF85063ATL RTC verified from Waveshare's official schematic and Arduino example. It uses I2C address `0x51` on the shared GPIO8/GPIO9 bus. At boot, a valid RTC value is loaded before Wi-Fi connects; successful NTP synchronization then corrects both the display and RTC. An invalid oscillator-stop value is rejected instead of being shown.

Hardware verification: after reset with board power maintained, the display showed `RTC` before changing to `NTP SYNC`, confirming RTC read-back and subsequent network correction. A CR927 cell is still required to verify retention across complete power removal.

The `settings-persistence` branch stores the 12/24-hour preference in ESP32 NVS. Settings now includes year selection from 2020 through 2069 with Gregorian leap-year handling. Saving manual time/date writes the complete value to the PCF85063 and labels the source `RTC / MANUAL`; the RTC then supplies that value after reset. Wi-Fi credentials remain in their separate gitignored header.

Hardware verification: manual RTC time/date/year and the NVS-backed 12/24-hour preference survived Reset, then NTP corrected the clock normally.

## Wi-Fi Credentials

Copy the structure from `include/wifi_credentials.h.example` into the gitignored `include/wifi_credentials.h`, then set `Ssid` and `Password`. An empty local credentials file is created during initial setup so the project builds in offline mode without exposing credentials.

Never commit `include/wifi_credentials.h`. The file is explicitly ignored by Git.

The initial LCD test showed an observed refresh callback rate of about 24 FPS, which matches the 21 MHz pixel clock and official porch timing currently in use. Some visible flicker may be expected at this bring-up stage.

## PlatformIO Configuration

There does not appear to be a standard exact PlatformIO board ID for the Waveshare `ESP32-S3-Touch-LCD-5B`, so this project includes a local board manifest at `boards/waveshare_esp32_s3_touch_lcd_5b.json`. It uses the generic ESP32-S3 Arduino variant while explicitly configuring the important board facts:

- `board = waveshare_esp32_s3_touch_lcd_5b`
- 16 MB flash
- 8 MB OPI PSRAM
- QIO flash mode
- USB CDC on boot for native USB serial
- upload and monitor ports are not fixed in `platformio.ini`; pass the current `/dev/ttyACM*` path on the command line when needed

The PlatformIO platform is pinned to the pioarduino ESP32 platform release for Arduino-ESP32 `3.0.7`, matching Waveshare's guidance to use Arduino ESP32 3.x and its FAQ note recommending Arduino ESP32 `3.0.7` for at least some examples.

LVGL is pinned to `lvgl/lvgl@8.4.0`, matching Waveshare's LVGL v8 example guidance for the ESP32-S3-Touch-LCD-5 family.

The display bring-up branch vendors the official Waveshare-bundled Arduino libraries under `lib/`:

- `ESP32_Display_Panel` 1.0.0
- `ESP32_IO_Expander` 1.0.1
- `esp-lib-utils` 0.1.2

These are kept local so the display test builds against the same versions shipped with Waveshare's example package.

For LCD and touch bring-up, `lib/ESP32_Display_Panel/esp_panel_drivers_conf.h` is intentionally narrowed to the required RGB bus, I2C bus, ST7262 LCD driver, GT911 touch driver, and CH422G IO expander. This keeps the Waveshare 1024x600 display path explicit and avoids required drivers being compiled without runtime creation support.

The touch serial test uses the official 5B touch details:

- GT911 capacitive touch controller
- I2C SDA GPIO 8
- I2C SCL GPIO 9
- GT911 I2C address `0x5D`
- interrupt GPIO 4
- reset through CH422G EXIO1

Hardware verification: the board successfully prints GT911 touch coordinates and release events over serial while the LCD color bars remain visible.

The LVGL bring-up test is intentionally minimal. It replaces the color-bar screen at boot with a simple LVGL scene and a touchable button. The older color-bar and touch-serial modules are still present as known-good hardware references.

Hardware verification: the board successfully displayed the `LVGL OK` screen and the touch button worked.

The `clock-prototype` branch now contains the first watch-face layout. This is still a rendering and interaction prototype; real time synchronization comes later.

UI code and temporary clock state live in `src/clock_ui.cpp`. The verified RGB panel, GT911 touch, and LVGL driver integration remain isolated in `src/lvgl_bringup.cpp`.

The Settings overlay is assembled while hidden and revealed only after all controls are ready, avoiding visible incremental redraws on the RGB panel.

LVGL uses a full 1024x600 RGB565 draw buffer in PSRAM so full-screen transitions flush as one frame instead of visible 40-row bands. If that allocation fails, firmware falls back to the smaller partial buffer and continues running.

## Build

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio run
```

## Upload

Do not upload without confirming with the project owner first.

Find the current native USB serial device:

```bash
ls -l /dev/ttyACM*
```

When approved, upload to the current native USB serial device. Example:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio run -t upload --upload-port /dev/ttyACM0
```

If the board re-enumerated as `/dev/ttyACM1`, use:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio run -t upload --upload-port /dev/ttyACM1
```

## Serial Monitor

After upload, open the serial monitor with the current ACM port. Example:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio device monitor --port /dev/ttyACM0 --baud 115200
```

If the board re-enumerated as `/dev/ttyACM1`, use:

```bash
PLATFORMIO_CORE_DIR=.pio .venv/bin/pio device monitor --port /dev/ttyACM1 --baud 115200
```

If the monitor disconnects after pressing reset, check `/dev/ttyACM*` again and restart the monitor with the new port.

## Planned Milestones

1. Build and upload the minimal serial sanity test. Done.
2. Find and verify the correct official Waveshare 1024x600 example. Done.
3. Display a simple image, color, or test pattern on the LCD. Done on `lcd-color-test`.
4. Bring up capacitive touch input. Done on `lcd-color-test`.
5. Add LVGL. Done on `lvgl-bringup`.
6. Build first simple clock screen. Done on `clock-prototype`.
7. Build the Casio F-91W-inspired clock UI. In progress on `clock-prototype`.
8. Add Wi-Fi/NTP time synchronization. In progress on `clock-prototype`.
9. Evaluate the onboard RTC and other peripherals. PCF85063 bring-up verified on `rtc-bringup`; power-loss retention remains to be tested with a CR927 cell.
10. Persist clock preferences and manual RTC settings. Done on `settings-persistence`.

## References

- Waveshare documentation for `ESP32-S3-Touch-LCD-5` family
- Waveshare official GitHub examples for `ESP32-S3-Touch-LCD-5`
- Espressif Arduino ESP32 framework documentation
- PlatformIO Espressif32 documentation
- Waveshare official ESP32-S3-Touch-LCD-5 schematic and `04_RTC_Test` example
