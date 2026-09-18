#pragma once

namespace ClockConfig {

// Enables automatic screensaver activation during the configured time window.
constexpr bool SleepModeEnabled = true;

// Use 24-hour local time in HH:MM format, including leading zeroes.
// Overnight windows are supported; for example, 22:00 to 07:00.
constexpr char SleepModeStart[] = "22:00";
constexpr char SleepModeEnd[] = "07:00";

}  // namespace ClockConfig
