#pragma once

#include <stdint.h>

#include <lvgl.h>

struct WeekdaySegmentGlyph {
  lv_obj_t *root = nullptr;
  lv_obj_t *segments[9] = {};
};

void createWeekdaySegmentGlyph(WeekdaySegmentGlyph &glyph, lv_obj_t *parent,
                               int x, int y, uint32_t color);
void setWeekdaySegmentGlyph(WeekdaySegmentGlyph &glyph, char letter);
