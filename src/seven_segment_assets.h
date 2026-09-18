#pragma once

#include <lvgl.h>

struct SevenSegmentAsset {
  const lv_img_dsc_t *image;
  lv_coord_t x;
  lv_coord_t y;
};

extern const lv_img_dsc_t segment_large_0;
extern const lv_img_dsc_t segment_large_1;
extern const lv_img_dsc_t segment_large_2;
extern const lv_img_dsc_t segment_large_3;
extern const lv_img_dsc_t segment_large_4;
extern const lv_img_dsc_t segment_large_5;
extern const lv_img_dsc_t segment_large_6;
extern const lv_img_dsc_t segment_small_0;
extern const lv_img_dsc_t segment_small_1;
extern const lv_img_dsc_t segment_small_2;
extern const lv_img_dsc_t segment_small_3;
extern const lv_img_dsc_t segment_small_4;
extern const lv_img_dsc_t segment_small_5;
extern const lv_img_dsc_t segment_small_6;
extern const lv_img_dsc_t segment_top_0;
extern const lv_img_dsc_t segment_top_1;
extern const lv_img_dsc_t segment_top_2;
extern const lv_img_dsc_t segment_top_3;
extern const lv_img_dsc_t segment_top_4;
extern const lv_img_dsc_t segment_top_5;
extern const lv_img_dsc_t segment_top_6;

extern const SevenSegmentAsset sevenSegmentAssetsLarge[7];
extern const SevenSegmentAsset sevenSegmentAssetsSmall[7];
extern const SevenSegmentAsset sevenSegmentAssetsTop[7];
