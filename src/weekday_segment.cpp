#include "weekday_segment.h"

#include "weekday_segment_assets.h"

namespace {
constexpr uint16_t segment(int index) { return 1U << index; }

uint16_t maskForLetter(char letter) {
  constexpr uint16_t A = segment(0);
  constexpr uint16_t B = segment(1);
  constexpr uint16_t C = segment(2);
  constexpr uint16_t D = segment(3);
  constexpr uint16_t E = segment(4);
  constexpr uint16_t F = segment(5);
  constexpr uint16_t G = segment(6);
  constexpr uint16_t H = segment(7);
  constexpr uint16_t I = segment(8);
  switch (letter) {
    case 'A': return A | B | C | E | F | G;
    case 'E': return A | D | E | F | G;
    case 'F': return A | E | F | G;
    case 'H': return B | C | E | F | G;
    case 'M': return A | B | C | E | F | H | I;
    case 'O': return A | B | C | D | E | F;
    case 'R': return A | B | C | E | F | G;
    case 'S': return A | C | D | F | G;
    case 'T': return A | H | I;
    case 'U': return B | C | D | E | F;
    case 'W': return B | C | D | E | F | H | I;
    default: return 0;
  }
}
}

void createWeekdaySegmentGlyph(WeekdaySegmentGlyph &glyph, lv_obj_t *parent,
                               int x, int y, uint32_t color) {
  glyph.root = lv_obj_create(parent);
  lv_obj_remove_style_all(glyph.root);
  lv_obj_set_pos(glyph.root, x, y);
  lv_obj_set_size(glyph.root, 60, 104);
  lv_obj_clear_flag(glyph.root, LV_OBJ_FLAG_CLICKABLE);
  for (int index = 0; index < 9; ++index) {
    glyph.segments[index] = lv_img_create(glyph.root);
    lv_img_set_src(glyph.segments[index], weekdaySegmentAssets[index].image);
    lv_obj_set_pos(glyph.segments[index], weekdaySegmentAssets[index].x,
                   weekdaySegmentAssets[index].y);
    lv_obj_set_style_img_recolor(glyph.segments[index], lv_color_hex(color), 0);
    lv_obj_set_style_img_recolor_opa(glyph.segments[index], LV_OPA_COVER, 0);
    lv_obj_set_style_img_opa(glyph.segments[index], 6, 0);
    lv_obj_clear_flag(glyph.segments[index], LV_OBJ_FLAG_CLICKABLE);
  }
}

void setWeekdaySegmentGlyph(WeekdaySegmentGlyph &glyph, char letter) {
  const uint16_t mask = maskForLetter(letter);
  for (int index = 0; index < 9; ++index) {
    lv_obj_set_style_img_opa(glyph.segments[index],
                             mask & (1U << index) ? 240 : 6, 0);
  }
}
