#include "clock_ui.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include "rtc_time.h"

namespace {
constexpr int ScreenWidth = 1024;
constexpr int ScreenHeight = 600;

lv_obj_t *statusLabel = nullptr;
lv_obj_t *timeLabel = nullptr;
lv_obj_t *dayLabel = nullptr;
lv_obj_t *dateLabel = nullptr;
lv_obj_t *modeLabel = nullptr;
lv_obj_t *timeSourceLabel = nullptr;
lv_obj_t *lcdPanel = nullptr;
lv_obj_t *configMenu = nullptr;
lv_obj_t *editHourLabel = nullptr;
lv_obj_t *editMinuteLabel = nullptr;
lv_obj_t *editPeriodButton = nullptr;
lv_obj_t *editPeriodLabel = nullptr;
lv_obj_t *format12Button = nullptr;
lv_obj_t *format24Button = nullptr;
lv_obj_t *editMonthLabel = nullptr;
lv_obj_t *editDayLabel = nullptr;
lv_obj_t *editWeekdayLabel = nullptr;
lv_obj_t *editYearLabel = nullptr;

unsigned long clockBaseMillis = 0;
unsigned long clockBaseSeconds = 0;
uint8_t editHour = 0;
uint8_t editMinute = 0;
bool illuminatorOn = false;
bool use24Hour = false;
bool editUse24Hour = false;
uint8_t calendarMonth = 1;
uint8_t calendarDay = 1;
uint8_t calendarWeekday = 0;
uint16_t calendarYear = 2026;
unsigned long calendarBaseDay = 0;
uint8_t editMonth = 1;
uint8_t editDay = 1;
uint8_t editWeekday = 0;
uint16_t editYear = 2026;

enum class TimeMenuAction : intptr_t {
  HourDown,
  HourUp,
  MinuteDown,
  MinuteUp,
  TogglePeriod,
  Format12,
  Format24,
  MonthNext,
  DayNext,
  WeekdayNext,
  YearNext,
  Cancel,
  Save,
};

constexpr const char *Weekdays[] = {"SUN", "MON", "TUE", "WED",
                                     "THU", "FRI", "SAT"};

uint8_t daysInMonth(uint8_t month, uint16_t year) {
  static constexpr uint8_t Days[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
    return 29;
  }
  return Days[month - 1];
}

void getCurrentDate(unsigned long currentDay, uint8_t &month, uint8_t &day,
                    uint8_t &weekday, uint16_t &year) {
  month = calendarMonth;
  day = calendarDay;
  weekday = calendarWeekday;
  year = calendarYear;
  unsigned long daysToAdvance = currentDay - calendarBaseDay;
  while (daysToAdvance-- > 0) {
    weekday = (weekday + 1) % 7;
    if (++day > daysInMonth(month, year)) {
      day = 1;
      if (++month > 12) {
        month = 1;
        ++year;
      }
    }
  }
}

lv_obj_t *createLabel(lv_obj_t *parent, const char *text,
                      const lv_font_t *font, uint32_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  return label;
}

unsigned long getClockSeconds() {
  return clockBaseSeconds + ((millis() - clockBaseMillis) / 1000);
}

void refreshClock() {
  if (timeLabel == nullptr) {
    return;
  }

  const unsigned long totalSeconds = getClockSeconds();
  const unsigned long hours = (totalSeconds / 3600) % 24;
  const unsigned long minutes = (totalSeconds / 60) % 60;
  const unsigned long seconds = totalSeconds % 60;
  if (use24Hour) {
    lv_label_set_text_fmt(timeLabel, "%02lu:%02lu:%02lu", hours, minutes,
                          seconds);
  } else {
    const unsigned long displayHour = (hours % 12) == 0 ? 12 : hours % 12;
    lv_label_set_text_fmt(timeLabel, "%02lu:%02lu:%02lu %s", displayHour,
                          minutes, seconds, hours < 12 ? "AM" : "PM");
  }

  lv_label_set_text(modeLabel, use24Hour ? "24H" : "12H");
  const unsigned long elapsedDays = totalSeconds / (24UL * 60UL * 60UL);
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint16_t year;
  getCurrentDate(elapsedDays, month, day, weekday, year);
  lv_label_set_text(dayLabel, Weekdays[weekday]);
  lv_label_set_text_fmt(dateLabel, "%u-%02u", static_cast<unsigned>(month),
                        static_cast<unsigned>(day));
}

void onScreenPressed(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED) {
    return;
  }

  illuminatorOn = !illuminatorOn;
  lv_obj_set_style_bg_color(
      lcdPanel, lv_color_hex(illuminatorOn ? 0xDCECA8 : 0xA8B58A), 0);
  lv_label_set_text(statusLabel,
                    illuminatorOn ? "ILLUMINATOR ON" : "TOUCH FOR LIGHT");
}

void closeConfigMenu() {
  if (configMenu == nullptr) {
    return;
  }
  lv_obj_add_flag(configMenu, LV_OBJ_FLAG_HIDDEN);
  lv_obj_del(configMenu);
  configMenu = nullptr;
  editHourLabel = nullptr;
  editMinuteLabel = nullptr;
  editPeriodButton = nullptr;
  editPeriodLabel = nullptr;
  format12Button = nullptr;
  format24Button = nullptr;
  editMonthLabel = nullptr;
  editDayLabel = nullptr;
  editWeekdayLabel = nullptr;
  editYearLabel = nullptr;
}

void updateTimeEditor() {
  const uint8_t displayHour =
      editUse24Hour ? editHour : ((editHour % 12) == 0 ? 12 : editHour % 12);
  lv_label_set_text_fmt(editHourLabel, "%02u",
                        static_cast<unsigned int>(displayHour));
  lv_label_set_text_fmt(editMinuteLabel, "%02u",
                        static_cast<unsigned int>(editMinute));
  lv_label_set_text(editPeriodLabel, editHour < 12 ? "AM" : "PM");

  if (editUse24Hour) {
    lv_obj_add_flag(editPeriodButton, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(editPeriodButton, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_bg_color(format12Button,
                            lv_color_hex(editUse24Hour ? 0x34383C : 0x287C8D),
                            0);
  lv_obj_set_style_bg_color(format24Button,
                            lv_color_hex(editUse24Hour ? 0x287C8D : 0x34383C),
                            0);
  lv_label_set_text_fmt(editMonthLabel, "MONTH %u",
                        static_cast<unsigned>(editMonth));
  lv_label_set_text_fmt(editDayLabel, "DAY %02u",
                        static_cast<unsigned>(editDay));
  lv_label_set_text(editWeekdayLabel, Weekdays[editWeekday]);
  lv_label_set_text_fmt(editYearLabel, "%u", static_cast<unsigned>(editYear));
}

void onTimeMenuAction(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  const auto action = static_cast<TimeMenuAction>(
      reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  switch (action) {
    case TimeMenuAction::HourDown:
      editHour = (editHour + 23) % 24;
      break;
    case TimeMenuAction::HourUp:
      editHour = (editHour + 1) % 24;
      break;
    case TimeMenuAction::MinuteDown:
      editMinute = (editMinute + 59) % 60;
      break;
    case TimeMenuAction::MinuteUp:
      editMinute = (editMinute + 1) % 60;
      break;
    case TimeMenuAction::TogglePeriod:
      editHour = (editHour + 12) % 24;
      break;
    case TimeMenuAction::Format12:
      editUse24Hour = false;
      break;
    case TimeMenuAction::Format24:
      editUse24Hour = true;
      break;
    case TimeMenuAction::MonthNext:
      editMonth = (editMonth % 12) + 1;
      if (editDay > daysInMonth(editMonth, editYear)) {
        editDay = daysInMonth(editMonth, editYear);
      }
      break;
    case TimeMenuAction::DayNext:
      editDay = (editDay % daysInMonth(editMonth, editYear)) + 1;
      break;
    case TimeMenuAction::WeekdayNext:
      editWeekday = (editWeekday + 1) % 7;
      break;
    case TimeMenuAction::YearNext:
      editYear = editYear >= 2069 ? 2020 : editYear + 1;
      if (editDay > daysInMonth(editMonth, editYear)) {
        editDay = daysInMonth(editMonth, editYear);
      }
      break;
    case TimeMenuAction::Cancel:
      closeConfigMenu();
      return;
    case TimeMenuAction::Save: {
      const unsigned long currentDays =
          getClockSeconds() / (24UL * 60UL * 60UL);
      clockBaseSeconds = currentDays * 24UL * 60UL * 60UL +
                         static_cast<unsigned long>(editHour) * 60UL * 60UL +
                         static_cast<unsigned long>(editMinute) * 60UL;
      clockBaseMillis = millis();
      use24Hour = editUse24Hour;
      calendarMonth = editMonth;
      calendarDay = editDay;
      calendarWeekday = editWeekday;
      calendarYear = editYear;
      calendarBaseDay = currentDays;

      Preferences preferences;
      if (preferences.begin("casio-clock", false)) {
        preferences.putBool("use24", use24Hour);
        preferences.end();
      }

      tm manualTime = {};
      manualTime.tm_year = editYear - 1900;
      manualTime.tm_mon = editMonth - 1;
      manualTime.tm_mday = editDay;
      manualTime.tm_wday = editWeekday;
      manualTime.tm_hour = editHour;
      manualTime.tm_min = editMinute;
      manualTime.tm_sec = 0;
      if (writeRtcDateTime(manualTime)) {
        setClockUiTimeSource("RTC / MANUAL");
      }
      refreshClock();
      closeConfigMenu();
      return;
    }
  }
  updateTimeEditor();
}

lv_obj_t *createMenuButton(lv_obj_t *parent, const char *text, int width,
                           int height, TimeMenuAction action) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x34383C), 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x4A5258), LV_STATE_PRESSED);
  lv_obj_set_style_radius(button, 5, 0);
  lv_obj_add_event_cb(button, onTimeMenuAction, LV_EVENT_CLICKED,
                      reinterpret_cast<void *>(static_cast<intptr_t>(action)));
  lv_obj_t *label = createLabel(button, text, &lv_font_montserrat_20, 0xF1F3F4);
  lv_obj_center(label);
  return button;
}

void openConfigMenu(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED || configMenu != nullptr) {
    return;
  }

  const unsigned long totalSeconds = getClockSeconds();
  editHour = (totalSeconds / 3600) % 24;
  editMinute = (totalSeconds / 60) % 60;
  editUse24Hour = use24Hour;
  const unsigned long currentDay = totalSeconds / (24UL * 60UL * 60UL);
  getCurrentDate(currentDay, editMonth, editDay, editWeekday, editYear);

  configMenu = lv_obj_create(lv_scr_act());
  lv_obj_add_flag(configMenu, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_style_all(configMenu);
  lv_obj_set_size(configMenu, ScreenWidth, ScreenHeight);
  lv_obj_set_style_bg_color(configMenu, lv_color_hex(0x17191B), 0);
  lv_obj_set_style_bg_opa(configMenu, LV_OPA_COVER, 0);
  lv_obj_add_flag(configMenu, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *title = createLabel(configMenu, "SETTINGS", &lv_font_montserrat_32,
                                0xF1F3F4);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 72, 30);
  lv_obj_t *section = createLabel(configMenu, "TIME & DATE",
                                  &lv_font_montserrat_20, 0x48B8D0);
  lv_obj_align(section, LV_ALIGN_TOP_LEFT, 74, 92);

  format12Button = createMenuButton(configMenu, "12 HOUR", 170, 54,
                                    TimeMenuAction::Format12);
  lv_obj_align(format12Button, LV_ALIGN_TOP_LEFT, 260, 115);
  format24Button = createMenuButton(configMenu, "24 HOUR", 170, 54,
                                    TimeMenuAction::Format24);
  lv_obj_align(format24Button, LV_ALIGN_TOP_RIGHT, -260, 115);

  lv_obj_t *panel = lv_obj_create(configMenu);
  lv_obj_set_size(panel, 760, 290);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 180);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x25282B), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x555B60), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 6, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *hourTitle = createLabel(panel, "HOUR", &lv_font_montserrat_14,
                                    0xB8BDC0);
  lv_obj_align(hourTitle, LV_ALIGN_TOP_LEFT, 150, 20);
  lv_obj_t *minuteTitle = createLabel(panel, "MINUTE", &lv_font_montserrat_14,
                                      0xB8BDC0);
  lv_obj_align(minuteTitle, LV_ALIGN_TOP_RIGHT, -138, 20);

  lv_obj_t *hourDown = createMenuButton(panel, "-", 92, 70,
                                        TimeMenuAction::HourDown);
  lv_obj_align(hourDown, LV_ALIGN_LEFT_MID, 44, -5);
  editHourLabel = createLabel(panel, "00", &lv_font_montserrat_48, 0xF1F3F4);
  lv_obj_align(editHourLabel, LV_ALIGN_CENTER, -185, -5);
  lv_obj_t *hourUp = createMenuButton(panel, "+", 92, 70,
                                      TimeMenuAction::HourUp);
  lv_obj_align(hourUp, LV_ALIGN_CENTER, -45, -5);
  lv_obj_t *separator = createLabel(panel, ":", &lv_font_montserrat_48,
                                    0xB8BDC0);
  lv_obj_align(separator, LV_ALIGN_CENTER, 0, -5);
  lv_obj_t *minuteDown = createMenuButton(panel, "-", 92, 70,
                                          TimeMenuAction::MinuteDown);
  lv_obj_align(minuteDown, LV_ALIGN_CENTER, 45, -5);
  editMinuteLabel = createLabel(panel, "00", &lv_font_montserrat_48, 0xF1F3F4);
  lv_obj_align(editMinuteLabel, LV_ALIGN_CENTER, 185, -5);
  lv_obj_t *minuteUp = createMenuButton(panel, "+", 92, 70,
                                        TimeMenuAction::MinuteUp);
  lv_obj_align(minuteUp, LV_ALIGN_RIGHT_MID, -44, -5);
  editPeriodButton = createMenuButton(panel, "AM", 110, 50,
                                      TimeMenuAction::TogglePeriod);
  lv_obj_align(editPeriodButton, LV_ALIGN_TOP_MID, 0, 12);
  editPeriodLabel = lv_obj_get_child(editPeriodButton, 0);

  lv_obj_t *monthButton = createMenuButton(panel, "MONTH 1", 150, 52,
                                           TimeMenuAction::MonthNext);
  lv_obj_align(monthButton, LV_ALIGN_BOTTOM_LEFT, 18, -12);
  editMonthLabel = lv_obj_get_child(monthButton, 0);
  lv_obj_t *dayButton = createMenuButton(panel, "DAY 01", 150, 52,
                                         TimeMenuAction::DayNext);
  lv_obj_align(dayButton, LV_ALIGN_BOTTOM_LEFT, 202, -12);
  editDayLabel = lv_obj_get_child(dayButton, 0);
  lv_obj_t *weekdayButton = createMenuButton(panel, "SUN", 150, 52,
                                             TimeMenuAction::WeekdayNext);
  lv_obj_align(weekdayButton, LV_ALIGN_BOTTOM_RIGHT, -202, -12);
  editWeekdayLabel = lv_obj_get_child(weekdayButton, 0);
  lv_obj_t *yearButton = createMenuButton(panel, "2026", 150, 52,
                                          TimeMenuAction::YearNext);
  lv_obj_align(yearButton, LV_ALIGN_BOTTOM_RIGHT, -18, -12);
  editYearLabel = lv_obj_get_child(yearButton, 0);

  lv_obj_t *cancel = createMenuButton(configMenu, "CANCEL", 170, 68,
                                      TimeMenuAction::Cancel);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 132, -42);
  lv_obj_t *save = createMenuButton(configMenu, "SAVE", 170, 68,
                                    TimeMenuAction::Save);
  lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, -132, -42);
  lv_obj_set_style_bg_color(save, lv_color_hex(0x287C8D), 0);
  updateTimeEditor();
  lv_obj_clear_flag(configMenu, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

void createClockUi() {
  Preferences preferences;
  if (preferences.begin("casio-clock", true)) {
    use24Hour = preferences.getBool("use24", false);
    preferences.end();
  }

  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x17191B), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(screen, onScreenPressed, LV_EVENT_PRESSED, nullptr);

  lv_obj_t *bezel = lv_obj_create(screen);
  lv_obj_remove_style_all(bezel);
  lv_obj_set_size(bezel, 860, 510);
  lv_obj_center(bezel);
  lv_obj_set_style_bg_color(bezel, lv_color_hex(0x303338), 0);
  lv_obj_set_style_bg_opa(bezel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(bezel, lv_color_hex(0x858A8D), 0);
  lv_obj_set_style_border_width(bezel, 8, 0);
  lv_obj_set_style_radius(bezel, 22, 0);
  lv_obj_clear_flag(bezel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *brand = createLabel(bezel, "DIGITAL", &lv_font_montserrat_20,
                                0xE5E7E8);
  lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 22);
  lv_obj_t *waterResist = createLabel(bezel, "WATER RESIST",
                                      &lv_font_montserrat_14, 0x48B8D0);
  lv_obj_align(waterResist, LV_ALIGN_TOP_LEFT, 86, 63);
  lv_obj_t *alarm = createLabel(bezel, "ALARM  CHRONOGRAPH",
                                &lv_font_montserrat_14, 0xD86A62);
  lv_obj_align(alarm, LV_ALIGN_TOP_RIGHT, -86, 63);

  lv_obj_t *accent = lv_obj_create(bezel);
  lv_obj_remove_style_all(accent);
  lv_obj_set_size(accent, 680, 4);
  lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_set_style_bg_color(accent, lv_color_hex(0xB94745), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);

  lcdPanel = lv_obj_create(bezel);
  lv_obj_remove_style_all(lcdPanel);
  lv_obj_set_size(lcdPanel, 710, 280);
  lv_obj_align(lcdPanel, LV_ALIGN_CENTER, 0, 18);
  lv_obj_set_style_bg_color(lcdPanel, lv_color_hex(0xA8B58A), 0);
  lv_obj_set_style_bg_opa(lcdPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(lcdPanel, lv_color_hex(0x111512), 0);
  lv_obj_set_style_border_width(lcdPanel, 12, 0);
  lv_obj_set_style_radius(lcdPanel, 5, 0);

  dayLabel = createLabel(lcdPanel, "SUN", &lv_font_montserrat_20, 0x20271D);
  lv_obj_align(dayLabel, LV_ALIGN_TOP_LEFT, 34, 28);
  dateLabel = createLabel(lcdPanel, "1-01", &lv_font_montserrat_20, 0x20271D);
  lv_obj_align(dateLabel, LV_ALIGN_TOP_RIGHT, -34, 28);
  modeLabel = createLabel(lcdPanel, "12H", &lv_font_montserrat_14, 0x20271D);
  lv_obj_align(modeLabel, LV_ALIGN_TOP_MID, 0, 32);
  timeLabel = createLabel(lcdPanel, "00:00:00", &lv_font_montserrat_48,
                          0x151B14);
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, 24);
  timeSourceLabel = createLabel(lcdPanel, "BOOT TIME", &lv_font_montserrat_14,
                                0x30382B);
  lv_obj_align(timeSourceLabel, LV_ALIGN_BOTTOM_MID, 0, -24);
  statusLabel = createLabel(bezel, "TOUCH FOR LIGHT", &lv_font_montserrat_14,
                            0xD2D5D6);
  lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -25);

  lv_obj_t *settingsHotspot = lv_obj_create(screen);
  lv_obj_remove_style_all(settingsHotspot);
  lv_obj_set_size(settingsHotspot, 200, 200);
  lv_obj_align(settingsHotspot, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_flag(settingsHotspot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(settingsHotspot, openConfigMenu, LV_EVENT_CLICKED, nullptr);
  refreshClock();
}

void updateClockUi() { refreshClock(); }

void setClockUiDateTime(const tm &dateTime) {
  const unsigned long currentDays =
      getClockSeconds() / (24UL * 60UL * 60UL);
  clockBaseSeconds = currentDays * 24UL * 60UL * 60UL +
                     static_cast<unsigned long>(dateTime.tm_hour) * 3600UL +
                     static_cast<unsigned long>(dateTime.tm_min) * 60UL +
                     static_cast<unsigned long>(dateTime.tm_sec);
  clockBaseMillis = millis();
  calendarMonth = dateTime.tm_mon + 1;
  calendarDay = dateTime.tm_mday;
  calendarWeekday = dateTime.tm_wday;
  calendarYear = dateTime.tm_year + 1900;
  calendarBaseDay = currentDays;
  refreshClock();
}

void setClockUiTimeSource(const char *source) {
  if (timeSourceLabel != nullptr) {
    lv_label_set_text(timeSourceLabel, source);
  }
}
