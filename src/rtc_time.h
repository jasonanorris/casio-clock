#pragma once

#include <time.h>

bool initRtcTime();
bool rtcTimeAvailable();
bool readRtcDateTime(tm &dateTime);
bool writeRtcDateTime(const tm &dateTime);
