# Third-Party Notices

This project uses and vendors third-party code under compatible open-source
licenses.

## Waveshare examples and documentation

Hardware configuration in this project was derived from Waveshare's public
documentation and official example code for the ESP32-S3-Touch-LCD-5 family.
Waveshare's ESP32-S3-Touch-LCD-5 example repository is licensed under the
Apache License 2.0.

- Upstream repository: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-5
- Product documentation: https://docs.waveshare.com/ESP32-S3-Touch-LCD-5

## Vendored libraries

The following libraries are vendored under `lib/` for reproducible display and
touch bring-up:

- `ESP32_Display_Panel` 1.0.0, Apache License 2.0
- `ESP32_IO_Expander` 1.0.1, Apache License 2.0
- `esp-lib-utils` 0.1.2, Apache License 2.0

Their license files are preserved in each vendored library directory.

`ESP32_Display_Panel` also contains an XPT2046 touch driver marked
`SPDX-License-Identifier: MIT`; that notice is preserved in the vendored source
files under `lib/ESP32_Display_Panel/src/drivers/touch/port/`.

## Downloaded dependencies

The firmware depends on LVGL 8.4.0 through PlatformIO. LVGL is MIT licensed and
is not vendored in this repository; PlatformIO downloads it during the build.
