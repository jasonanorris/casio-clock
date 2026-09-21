# casio-clock

A standalone, oversized Casio F-91W-inspired digital clock for the Waveshare
ESP32-S3 5-inch capacitive touchscreen board.

The full 1024x600 display is used as the watch LCD face: large segmented time,
weekday, day number, AM/PM or 24H indicator, touch settings, NTP time sync, RTC
support, and a simple dark screensaver.

## Hardware

This project targets the Waveshare `ESP32-S3-Touch-LCD-5B`:

- ESP32-S3-WROOM-1-N16R8
- 16 MB flash
- 8 MB PSRAM
- 5-inch 1024x600 RGB LCD
- Capacitive touch
- Native ESP32-S3 USB serial/JTAG
- PCF85063ATL RTC

Important: this is for the 1024x600 `ESP32-S3-Touch-LCD-5B` model, not the
800x480 `ESP32-S3-Touch-LCD-5`. The LCD timings, GPIO mappings, and examples
are not interchangeable.

## What Works

- F-91W-inspired LCD-style clock face
- 12-hour and 24-hour display modes
- Touch settings menu from an invisible upper-right hotspot
- Manual time/date setting
- Wi-Fi/NTP synchronization
- RTC read/write support
- Persistent settings using ESP32 NVS
- Dark bouncing-time screensaver from an invisible upper-left hotspot
- Optional sleep-mode schedule for the screensaver
- Desktop SDL2 simulator for UI work without flashing the board

## Requirements

- Git
- PlatformIO CLI
- A USB-C data cable
- Linux, macOS, or Windows

The examples below use Linux shell commands. On Linux, the board usually appears
as `/dev/ttyACM0`, but it can re-enumerate as `/dev/ttyACM1` after reset.

On Debian/Ubuntu/Linux Mint, you may need serial-device permissions:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in after changing groups.

## Quick Start

Clone the repo:

```bash
git clone https://github.com/jasonanorris/casio-clock.git
cd casio-clock
```

Install PlatformIO if you do not already have it:

```bash
python3 -m pip install --user platformio
```

Build the firmware:

```bash
pio run
```

Find the board port:

```bash
ls -l /dev/ttyACM*
```

Upload to the board:

```bash
pio run -t upload --upload-port /dev/ttyACM0
```

Open the serial monitor:

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

If your board appears as `/dev/ttyACM1`, use that path in the upload and monitor
commands instead.

## Wi-Fi Setup

Wi-Fi credentials are optional. A fresh clone builds with Wi-Fi disabled by
using `include/wifi_credentials.h.example` as a fallback. Without credentials,
the clock still runs locally and can be set manually from the settings menu.

To enable NTP time sync, copy the example credentials file:

```bash
cp include/wifi_credentials.h.example include/wifi_credentials.h
```

Edit `include/wifi_credentials.h`:

```cpp
#pragma once

namespace WifiCredentials {
constexpr char Ssid[] = "YOUR_WIFI_NAME";
constexpr char Password[] = "YOUR_WIFI_PASSWORD";
}  // namespace WifiCredentials
```

`include/wifi_credentials.h` is ignored by Git so local network credentials do
not get committed.

## Timezone And Defaults

Clock behavior defaults live in `include/clock_config.h`.

The default timezone is Central Time for `America/Chicago`:

```cpp
constexpr char Timezone[] = "CST6CDT,M3.2.0,M11.1.0";
```

The ESP32 time API uses POSIX timezone rules instead of names like
`America/Chicago`. To find another value, search the Arduino timezone list:

https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h

For example, search that file for `TZ_America_New_York` and copy the string
inside `PSTR("...")`.

The same file also contains the default sleep-mode screensaver schedule:

```cpp
constexpr bool SleepModeEnabled = true;
constexpr char SleepModeStart[] = "22:00";
constexpr char SleepModeEnd[] = "07:00";
```

Sleep-mode times use local 24-hour `HH:MM` format. Overnight windows are
supported.

Saved settings on the ESP32 override these compile-time defaults after the first
time they are changed from the settings menu.

## Controls

- Upper-right 200x200 area: open Settings
- Upper-left 200x200 area: open Screensaver
- Tap the screensaver: return to the clock

The hotspots are intentionally invisible so the normal clock face looks like a
plain LCD panel.

## Desktop Simulator

The simulator renders the same clock UI in a 1024x600 SDL2 window. It is useful
for layout work, but it does not emulate the Waveshare RGB panel, GT911 touch
controller, Wi-Fi, NVS, or RTC hardware.

Install host dependencies on Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential cmake libsdl2-dev
```

Configure and build:

```bash
cmake -S simulator -B simulator/build
cmake --build simulator/build --target casio-clock-sim -j
```

Run:

```bash
./simulator/build/casio-clock-sim
```

Mouse clicks act like touch input. Simulator settings last only for the current
process.

## Project Layout

- `platformio.ini` - PlatformIO environment and pinned dependencies
- `boards/waveshare_esp32_s3_touch_lcd_5b.json` - local board definition
- `include/clock_config.h` - user-editable clock defaults
- `include/wifi_credentials.h.example` - Wi-Fi credentials template
- `src/lvgl_bringup.cpp` - LCD, touch, and LVGL hardware setup
- `src/clock_ui.cpp` - clock face, settings UI, and screensaver
- `src/network_time.cpp` - Wi-Fi and NTP synchronization
- `src/rtc_time.cpp` - PCF85063 RTC support
- `simulator/` - desktop SDL2 simulator
- `assets/` and `tools/` - source SVGs and generators for segmented digits

## Platform Notes

This repo includes a local PlatformIO board manifest because PlatformIO does not
provide an exact standard board definition for this Waveshare 1024x600 board.
The project explicitly configures:

- 16 MB flash
- 8 MB OPI PSRAM
- QIO flash mode
- native USB CDC serial on boot

The firmware currently uses:

- Arduino ESP32 `3.0.7` through the pinned pioarduino platform
- LVGL `8.4.0`
- Waveshare-bundled display libraries vendored in `lib/`

The display path uses verified Waveshare 1024x600 `5B` values for the RGB LCD,
GT911 touch controller, and CH422G IO expander. Do not replace them with values
from the 800x480 model.

## Troubleshooting

If upload fails, check that the port exists:

```bash
ls -l /dev/ttyACM*
```

If the serial monitor disconnects after reset, the board probably re-enumerated.
Run `ls -l /dev/ttyACM*` again and reopen the monitor with the new port.

If PlatformIO cannot find `pio`, close and reopen your terminal after installing
it, or use the full path shown by:

```bash
python3 -m site --user-base
```

If NTP never syncs, confirm `include/wifi_credentials.h` exists, the SSID and
password are correct, and the board has Wi-Fi signal.

## License

This project is licensed under the Apache License 2.0. See `LICENSE`.

Third-party code and notices are listed in `THIRD_PARTY_NOTICES.md`. Vendored
libraries under `lib/` retain their own license files and source notices.

This project is inspired by the look of classic Casio F-91W digital watches, but
it is not affiliated with or endorsed by Casio.

## References

- Waveshare documentation for the ESP32-S3-Touch-LCD-5 family
- Waveshare official ESP32-S3-Touch-LCD-5 examples
- Espressif Arduino ESP32 documentation
- PlatformIO Espressif32 documentation
