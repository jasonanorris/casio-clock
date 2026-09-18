#pragma once

#include <time.h>

bool initRtcTime();
bool readRtcDateTime(tm &dateTime);
bool writeRtcDateTime(const tm &dateTime);
