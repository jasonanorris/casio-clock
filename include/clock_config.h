#pragma once

namespace ClockConfig {

// POSIX timezone rule used by ESP32 configTzTime().
// America/Chicago: Central Time with US daylight saving rules.
// To find another value, search the Arduino TZ list:
// https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
constexpr char Timezone[] = "CST6CDT,M3.2.0,M11.1.0";

// Enables automatic screensaver activation during the configured time window.
constexpr bool SleepModeEnabled = true;

// Use 24-hour local time in HH:MM format, including leading zeroes.
// Overnight windows are supported; for example, 22:00 to 07:00.
constexpr char SleepModeStart[] = "22:00";
constexpr char SleepModeEnd[] = "07:00";

}  // namespace ClockConfig
