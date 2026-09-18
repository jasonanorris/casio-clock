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

bool clockPlatformWriteRtc(const tm &dateTime) {
  return writeRtcDateTime(dateTime);
}
