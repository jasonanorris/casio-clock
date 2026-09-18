#pragma once

#include <stdint.h>
#include <time.h>

struct ClockSleepSettings {
  bool enabled;
  uint16_t startMinutes;
  uint16_t endMinutes;
};

uint32_t clockPlatformMillis();
bool clockPlatformLoadUse24Hour();
void clockPlatformSaveUse24Hour(bool use24Hour);
ClockSleepSettings clockPlatformLoadSleepSettings(
    const ClockSleepSettings &defaults);
void clockPlatformSaveSleepSettings(const ClockSleepSettings &settings);
bool clockPlatformWriteRtc(const tm &dateTime);
