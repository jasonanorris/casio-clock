#pragma once

#include <time.h>

void createClockUi();
void updateClockUi();
void setClockUiDateTime(const tm &dateTime);
void setClockUiTimeSource(const char *source);
