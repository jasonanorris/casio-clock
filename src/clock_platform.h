#pragma once

#include <stdint.h>
#include <time.h>

uint32_t clockPlatformMillis();
bool clockPlatformLoadUse24Hour();
void clockPlatformSaveUse24Hour(bool use24Hour);
bool clockPlatformWriteRtc(const tm &dateTime);
