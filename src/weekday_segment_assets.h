#pragma once

#include <lvgl.h>

struct WeekdaySegmentAsset {
  const lv_img_dsc_t *image;
  lv_coord_t x;
  lv_coord_t y;
};

extern const lv_img_dsc_t weekday_segment_0;
extern const lv_img_dsc_t weekday_segment_1;
extern const lv_img_dsc_t weekday_segment_2;
extern const lv_img_dsc_t weekday_segment_3;
extern const lv_img_dsc_t weekday_segment_4;
extern const lv_img_dsc_t weekday_segment_5;
extern const lv_img_dsc_t weekday_segment_6;
extern const lv_img_dsc_t weekday_segment_7;
extern const lv_img_dsc_t weekday_segment_8;

extern const WeekdaySegmentAsset weekdaySegmentAssets[9];
