#pragma once

#include <stdint.h>

#include <lvgl.h>

struct SevenSegmentDigit {
  lv_obj_t *root = nullptr;
  lv_obj_t *segments[7] = {};
};

enum class SevenSegmentProfile {
  Large,
  Small,
  Top,
};

void createSevenSegmentDigit(SevenSegmentDigit &digit, lv_obj_t *parent,
                             int x, int y, int width, int height,
                             SevenSegmentProfile profile, uint32_t color);
void setSevenSegmentDigit(SevenSegmentDigit &digit, uint8_t value);
