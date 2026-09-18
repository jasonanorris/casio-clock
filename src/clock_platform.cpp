#include "clock_platform.h"

#include <Arduino.h>
#include <Preferences.h>

#include "rtc_time.h"

uint32_t clockPlatformMillis() { return millis(); }

bool clockPlatformLoadUse24Hour() {
  Preferences preferences;
  if (!preferences.begin("casio-clock", true)) return false;
  const bool use24Hour = preferences.getBool("use24", false);
  preferences.end();
  return use24Hour;
}

void clockPlatformSaveUse24Hour(bool use24Hour) {
  Preferences preferences;
  if (preferences.begin("casio-clock", false)) {
    preferences.putBool("use24", use24Hour);
    preferences.end();
  }
}

ClockSleepSettings clockPlatformLoadSleepSettings(
    const ClockSleepSettings &defaults) {
  Preferences preferences;
  if (!preferences.begin("casio-clock", true)) return defaults;
  ClockSleepSettings settings = {
      preferences.getBool("sleepOn", defaults.enabled),
      preferences.getUShort("sleepStart", defaults.startMinutes),
      preferences.getUShort("sleepEnd", defaults.endMinutes),
  };
  preferences.end();
  if (settings.startMinutes >= 24 * 60 || settings.endMinutes >= 24 * 60) {
    return defaults;
  }
  return settings;
}

void clockPlatformSaveSleepSettings(const ClockSleepSettings &settings) {
  Preferences preferences;
  if (preferences.begin("casio-clock", false)) {
    preferences.putBool("sleepOn", settings.enabled);
    preferences.putUShort("sleepStart", settings.startMinutes);
    preferences.putUShort("sleepEnd", settings.endMinutes);
    preferences.end();
  }
}

bool clockPlatformWriteRtc(const tm &dateTime) {
  return writeRtcDateTime(dateTime);
}
