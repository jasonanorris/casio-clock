#include "clock_platform.h"

#include <chrono>

namespace {
const auto StartedAt = std::chrono::steady_clock::now();
bool savedUse24Hour = false;
}

uint32_t clockPlatformMillis() {
  const auto elapsed = std::chrono::steady_clock::now() - StartedAt;
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}
bool clockPlatformLoadUse24Hour() { return savedUse24Hour; }
void clockPlatformSaveUse24Hour(bool use24Hour) { savedUse24Hour = use24Hour; }
bool clockPlatformWriteRtc(const tm &) { return false; }
