#include "seven_segment.h"

#include "seven_segment_assets.h"

namespace {
constexpr uint8_t DigitMasks[] = {
    0x3F,  // 0: A B C D E F
    0x06,  // 1: B C
    0x5B,  // 2: A B D E G
    0x4F,  // 3: A B C D G
    0x66,  // 4: B C F G
    0x6D,  // 5: A C D F G
    0x7D,  // 6: A C D E F G
    0x07,  // 7: A B C
    0x7F,  // 8: all
    0x6F,  // 9: A B C D F G
};
}

void createSevenSegmentDigit(SevenSegmentDigit &digit, lv_obj_t *parent,
                             int x, int y, int width, int height,
                             SevenSegmentProfile profile, uint32_t color) {
  digit.root = lv_obj_create(parent);
  lv_obj_remove_style_all(digit.root);
  lv_obj_set_pos(digit.root, x, y);
  lv_obj_set_size(digit.root, width, height);
  lv_obj_clear_flag(digit.root, LV_OBJ_FLAG_CLICKABLE);

  const SevenSegmentAsset *assets = sevenSegmentAssetsLarge;
  switch (profile) {
    case SevenSegmentProfile::Large:
      break;
    case SevenSegmentProfile::Small:
      assets = sevenSegmentAssetsSmall;
      break;
    case SevenSegmentProfile::Top:
      assets = sevenSegmentAssetsTop;
      break;
  }

  for (int index = 0; index < 7; ++index) {
    digit.segments[index] = lv_img_create(digit.root);
    lv_img_set_src(digit.segments[index], assets[index].image);
    lv_obj_set_pos(digit.segments[index], assets[index].x, assets[index].y);
    lv_obj_set_style_img_recolor(digit.segments[index], lv_color_hex(color), 0);
    lv_obj_set_style_img_recolor_opa(digit.segments[index], LV_OPA_COVER, 0);
    lv_obj_set_style_img_opa(digit.segments[index], 6, 0);
    lv_obj_clear_flag(digit.segments[index], LV_OBJ_FLAG_CLICKABLE);
  }
}

void setSevenSegmentDigit(SevenSegmentDigit &digit, uint8_t value) {
  const uint8_t mask = value < 10 ? DigitMasks[value] : 0;
  for (int index = 0; index < 7; ++index) {
    lv_obj_set_style_img_opa(digit.segments[index],
                             mask & (1U << index) ? 240 : 6, 0);
  }
}
