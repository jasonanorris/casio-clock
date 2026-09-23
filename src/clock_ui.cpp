#include "clock_ui.h"

#include <lvgl.h>

#include "clock_config.h"
#include "clock_platform.h"
#include "seven_segment.h"
#include "weekday_segment.h"

namespace {
constexpr int ScreenWidth = 1024;
constexpr int ScreenHeight = 600;

lv_obj_t *modeLabel = nullptr;
lv_obj_t *modeBoldLabel = nullptr;
lv_obj_t *lcdPanel = nullptr;
lv_obj_t *configMenu = nullptr;
lv_obj_t *screensaver = nullptr;
lv_obj_t *screensaverTime = nullptr;
lv_obj_t *screensaverDots[2] = {};
lv_timer_t *screensaverTimer = nullptr;
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
lv_obj_t *timeDatePage = nullptr;
lv_obj_t *screensaverPage = nullptr;
lv_obj_t *timeDateNavButton = nullptr;
lv_obj_t *screensaverNavButton = nullptr;
lv_obj_t *sleepModeButton = nullptr;
lv_obj_t *sleepModeLabel = nullptr;
lv_obj_t *sleepStartLabel = nullptr;
lv_obj_t *sleepEndLabel = nullptr;
SevenSegmentDigit timeDigits[4];
SevenSegmentDigit secondDigits[2];
SevenSegmentDigit dateDigits[2];
WeekdaySegmentGlyph weekdayGlyphs[2];
SevenSegmentDigit screensaverDigits[4];

unsigned long clockBaseMillis = 0;
unsigned long clockBaseSeconds = 0;
uint8_t editHour = 0;
uint8_t editMinute = 0;
bool illuminatorOn = false;
bool use24Hour = false;
bool editUse24Hour = false;
bool screensaverAutomatic = false;
bool sleepModeDismissed = false;
bool sleepModeEnabled = false;
bool editSleepModeEnabled = false;
bool editDateTimeChanged = false;
uint16_t sleepModeStartMinutes = 22 * 60;
uint16_t sleepModeEndMinutes = 7 * 60;
uint16_t editSleepStartMinutes = 22 * 60;
uint16_t editSleepEndMinutes = 7 * 60;
uint8_t calendarMonth = 1;
uint8_t calendarDay = 1;
uint8_t calendarWeekday = 0;
uint16_t calendarYear = 2026;
unsigned long calendarBaseDay = 0;
uint8_t editMonth = 1;
uint8_t editDay = 1;
uint8_t editWeekday = 0;
uint16_t editYear = 2026;
int screensaverX = 72;
int screensaverY = 72;
int screensaverVelocityX = 1;
int screensaverVelocityY = 1;

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
  ShowTimeDate,
  ShowScreensaver,
  ToggleSleepMode,
  SleepStartHourDown,
  SleepStartHourUp,
  SleepStartMinuteDown,
  SleepStartMinuteUp,
  SleepEndHourDown,
  SleepEndHourUp,
  SleepEndMinuteDown,
  SleepEndMinuteUp,
  Cancel,
  Save,
};

constexpr const char *Weekdays[] = {"SUN", "MON", "TUE", "WED",
                                     "THU", "FRI", "SAT"};
constexpr const char *LcdWeekdays[] = {"SU", "MO", "TU", "WE",
                                       "TH", "FR", "SA"};

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

lv_obj_t *createLcdDot(lv_obj_t *parent, int x, int y, int size) {
  lv_obj_t *dot = lv_obj_create(parent);
  lv_obj_remove_style_all(dot);
  lv_obj_set_pos(dot, x, y);
  lv_obj_set_size(dot, size, size);
  lv_obj_set_style_bg_color(dot, lv_color_hex(0x263129), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_90, 0);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  return dot;
}

unsigned long getClockSeconds() {
  return clockBaseSeconds + ((clockPlatformMillis() - clockBaseMillis) / 1000);
}

int parseClockMinutes(const char *time) {
  if (time[0] < '0' || time[0] > '2' || time[1] < '0' || time[1] > '9' ||
      time[2] != ':' || time[3] < '0' || time[3] > '5' || time[4] < '0' ||
      time[4] > '9' || time[5] != '\0') {
    return -1;
  }
  const int hour = (time[0] - '0') * 10 + (time[1] - '0');
  const int minute = (time[3] - '0') * 10 + (time[4] - '0');
  return hour < 24 ? hour * 60 + minute : -1;
}

bool isSleepModeTime(unsigned long totalSeconds) {
  if (!sleepModeEnabled || sleepModeStartMinutes == sleepModeEndMinutes) {
    return false;
  }
  const int start = sleepModeStartMinutes;
  const int end = sleepModeEndMinutes;
  const int now = (totalSeconds / 60) % (24 * 60);
  return start < end ? now >= start && now < end : now >= start || now < end;
}

void updateScreensaverTime(unsigned long totalSeconds) {
  if (screensaverTime == nullptr) {
    return;
  }

  const unsigned long hours = (totalSeconds / 3600) % 24;
  const unsigned long minutes = (totalSeconds / 60) % 60;
  const unsigned long displayHour =
      use24Hour ? hours : ((hours % 12) == 0 ? 12 : hours % 12);
  const bool twoDigitHour = displayHour >= 10;
  const int hourOnesX = twoDigitHour ? 120 : 0;
  const int colonX = twoDigitHour ? 248 : 128;
  const int minuteTensX = twoDigitHour ? 288 : 168;
  const int minuteOnesX = twoDigitHour ? 408 : 288;

  if (twoDigitHour) {
    lv_obj_clear_flag(screensaverDigits[0].root, LV_OBJ_FLAG_HIDDEN);
    setSevenSegmentDigit(screensaverDigits[0], displayHour / 10);
  } else {
    lv_obj_add_flag(screensaverDigits[0].root, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_x(screensaverDigits[1].root, hourOnesX);
  lv_obj_set_x(screensaverDots[0], colonX);
  lv_obj_set_x(screensaverDots[1], colonX);
  lv_obj_set_x(screensaverDigits[2].root, minuteTensX);
  lv_obj_set_x(screensaverDigits[3].root, minuteOnesX);
  lv_obj_set_width(screensaverTime, twoDigitHour ? 520 : 400);

  setSevenSegmentDigit(screensaverDigits[1], displayHour % 10);
  setSevenSegmentDigit(screensaverDigits[2], minutes / 10);
  setSevenSegmentDigit(screensaverDigits[3], minutes % 10);
}

void moveScreensaver(lv_timer_t *) {
  if (screensaverTime == nullptr) {
    return;
  }

  const int maxX = ScreenWidth - lv_obj_get_width(screensaverTime);
  const int maxY = ScreenHeight - lv_obj_get_height(screensaverTime);
  screensaverX += screensaverVelocityX;
  screensaverY += screensaverVelocityY;

  if (screensaverX <= 0 || screensaverX >= maxX) {
    screensaverX = screensaverX <= 0 ? 0 : maxX;
    screensaverVelocityX = -screensaverVelocityX;
  }
  if (screensaverY <= 0 || screensaverY >= maxY) {
    screensaverY = screensaverY <= 0 ? 0 : maxY;
    screensaverVelocityY = -screensaverVelocityY;
  }
  lv_obj_set_pos(screensaverTime, screensaverX, screensaverY);
}

void hideScreensaver(bool dismissSleepWindow) {
  if (screensaverTimer != nullptr) {
    lv_timer_del(screensaverTimer);
    screensaverTimer = nullptr;
  }
  lv_obj_del(screensaver);
  screensaver = nullptr;
  screensaverTime = nullptr;
  screensaverDots[0] = nullptr;
  screensaverDots[1] = nullptr;
  screensaverAutomatic = false;
  sleepModeDismissed = dismissSleepWindow;
}

void closeScreensaver(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED || screensaver == nullptr) {
    return;
  }
  hideScreensaver(isSleepModeTime(getClockSeconds()));
}

void showScreensaver(bool automatic) {
  if (screensaver != nullptr || configMenu != nullptr) {
    return;
  }

  screensaver = lv_obj_create(lv_scr_act());
  lv_obj_add_flag(screensaver, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_style_all(screensaver);
  lv_obj_set_size(screensaver, ScreenWidth, ScreenHeight);
  lv_obj_set_style_bg_color(screensaver, lv_color_hex(0x101210), 0);
  lv_obj_set_style_bg_opa(screensaver, LV_OPA_COVER, 0);
  lv_obj_add_flag(screensaver, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(screensaver, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(screensaver, closeScreensaver, LV_EVENT_CLICKED, nullptr);

  screensaverTime = lv_obj_create(screensaver);
  lv_obj_remove_style_all(screensaverTime);
  lv_obj_set_size(screensaverTime, 520, 221);
  lv_obj_clear_flag(screensaverTime, LV_OBJ_FLAG_CLICKABLE);
  constexpr uint32_t ScreensaverColor = 0xB7BBB7;
  createSevenSegmentDigit(screensaverDigits[0], screensaverTime, 0, 0, 112,
                          221, SevenSegmentProfile::Small, ScreensaverColor);
  createSevenSegmentDigit(screensaverDigits[1], screensaverTime, 120, 0, 112,
                          221, SevenSegmentProfile::Small, ScreensaverColor);
  screensaverDots[0] = createLcdDot(screensaverTime, 248, 69, 16);
  screensaverDots[1] = createLcdDot(screensaverTime, 248, 136, 16);
  for (lv_obj_t *dot : screensaverDots) {
    lv_obj_set_style_bg_color(dot, lv_color_hex(ScreensaverColor), 0);
  }
  createSevenSegmentDigit(screensaverDigits[2], screensaverTime, 288, 0, 112,
                          221, SevenSegmentProfile::Small, ScreensaverColor);
  createSevenSegmentDigit(screensaverDigits[3], screensaverTime, 408, 0, 112,
                          221, SevenSegmentProfile::Small, ScreensaverColor);
  updateScreensaverTime(getClockSeconds());
  screensaverX = 72;
  screensaverY = 72;
  screensaverVelocityX = 1;
  screensaverVelocityY = 1;
  lv_obj_set_pos(screensaverTime, screensaverX, screensaverY);
  screensaverTimer = lv_timer_create(moveScreensaver, 40, nullptr);
  screensaverAutomatic = automatic;
  lv_obj_clear_flag(screensaver, LV_OBJ_FLAG_HIDDEN);
}

void openScreensaver(lv_event_t *event) {
  // A sleep-window dismissal only suppresses automatic activation. The
  // upper-left hotspot must always remain available for manual activation.
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    showScreensaver(false);
  }
}

void updateSleepMode(unsigned long totalSeconds) {
  const bool sleepTime = isSleepModeTime(totalSeconds);
  if (!sleepTime) {
    sleepModeDismissed = false;
    if (screensaverAutomatic) {
      hideScreensaver(false);
    }
    return;
  }
  if (screensaver != nullptr) {
    screensaverAutomatic = true;
  }
  if (screensaver == nullptr && !sleepModeDismissed) {
    showScreensaver(true);
  }
}

void refreshClock() {
  if (timeDigits[0].root == nullptr) {
    return;
  }

  const unsigned long totalSeconds = getClockSeconds();
  const unsigned long hours = (totalSeconds / 3600) % 24;
  const unsigned long minutes = (totalSeconds / 60) % 60;
  const unsigned long seconds = totalSeconds % 60;
  updateSleepMode(totalSeconds);
  updateScreensaverTime(totalSeconds);
  const unsigned long displayHour =
      use24Hour ? hours : ((hours % 12) == 0 ? 12 : hours % 12);
  if (displayHour < 10) {
    lv_obj_add_flag(timeDigits[0].root, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(timeDigits[0].root, LV_OBJ_FLAG_HIDDEN);
    setSevenSegmentDigit(timeDigits[0], displayHour / 10);
  }
  setSevenSegmentDigit(timeDigits[1], displayHour % 10);
  setSevenSegmentDigit(timeDigits[2], minutes / 10);
  setSevenSegmentDigit(timeDigits[3], minutes % 10);
  setSevenSegmentDigit(secondDigits[0], seconds / 10);
  setSevenSegmentDigit(secondDigits[1], seconds % 10);
  const char *modeText = use24Hour ? "24H" : (hours < 12 ? "AM" : "PM");
  lv_label_set_text(modeLabel, modeText);
  lv_label_set_text(modeBoldLabel, modeText);
  const unsigned long elapsedDays = totalSeconds / (24UL * 60UL * 60UL);
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint16_t year;
  getCurrentDate(elapsedDays, month, day, weekday, year);
  setWeekdaySegmentGlyph(weekdayGlyphs[0], LcdWeekdays[weekday][0]);
  setWeekdaySegmentGlyph(weekdayGlyphs[1], LcdWeekdays[weekday][1]);
  setSevenSegmentDigit(dateDigits[0], day / 10);
  setSevenSegmentDigit(dateDigits[1], day % 10);
}

void onScreenPressed(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED) {
    return;
  }

  illuminatorOn = !illuminatorOn;
  lv_obj_set_style_bg_color(
      lcdPanel, lv_color_hex(illuminatorOn ? 0xE3F0B5 : 0xB9C1A5), 0);
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
  timeDatePage = nullptr;
  screensaverPage = nullptr;
  timeDateNavButton = nullptr;
  screensaverNavButton = nullptr;
  sleepModeButton = nullptr;
  sleepModeLabel = nullptr;
  sleepStartLabel = nullptr;
  sleepEndLabel = nullptr;
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

void showSettingsPage(bool showScreensaverPage) {
  if (timeDatePage == nullptr || screensaverPage == nullptr) {
    return;
  }
  if (showScreensaverPage) {
    lv_obj_add_flag(timeDatePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(screensaverPage, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(timeDatePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screensaverPage, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_bg_color(
      timeDateNavButton,
      lv_color_hex(showScreensaverPage ? 0x34383C : 0x287C8D), 0);
  lv_obj_set_style_bg_color(
      screensaverNavButton,
      lv_color_hex(showScreensaverPage ? 0x287C8D : 0x34383C), 0);
}

void updateSleepEditor() {
  lv_label_set_text(sleepModeLabel, editSleepModeEnabled ? "ON" : "OFF");
  lv_obj_set_style_bg_color(
      sleepModeButton,
      lv_color_hex(editSleepModeEnabled ? 0x287C8D : 0x34383C), 0);
  lv_label_set_text_fmt(sleepStartLabel, "%02u:%02u",
                        editSleepStartMinutes / 60,
                        editSleepStartMinutes % 60);
  lv_label_set_text_fmt(sleepEndLabel, "%02u:%02u", editSleepEndMinutes / 60,
                        editSleepEndMinutes % 60);
}

uint16_t adjustMinutes(uint16_t value, int change) {
  return static_cast<uint16_t>((value + 24 * 60 + change) % (24 * 60));
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
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::HourUp:
      editHour = (editHour + 1) % 24;
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::MinuteDown:
      editMinute = (editMinute + 59) % 60;
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::MinuteUp:
      editMinute = (editMinute + 1) % 60;
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::TogglePeriod:
      editHour = (editHour + 12) % 24;
      editDateTimeChanged = true;
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
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::DayNext:
      editDay = (editDay % daysInMonth(editMonth, editYear)) + 1;
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::WeekdayNext:
      editWeekday = (editWeekday + 1) % 7;
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::YearNext:
      editYear = editYear >= 2069 ? 2020 : editYear + 1;
      if (editDay > daysInMonth(editMonth, editYear)) {
        editDay = daysInMonth(editMonth, editYear);
      }
      editDateTimeChanged = true;
      break;
    case TimeMenuAction::ShowTimeDate:
      showSettingsPage(false);
      return;
    case TimeMenuAction::ShowScreensaver:
      showSettingsPage(true);
      return;
    case TimeMenuAction::ToggleSleepMode:
      editSleepModeEnabled = !editSleepModeEnabled;
      break;
    case TimeMenuAction::SleepStartHourDown:
      editSleepStartMinutes = adjustMinutes(editSleepStartMinutes, -60);
      break;
    case TimeMenuAction::SleepStartHourUp:
      editSleepStartMinutes = adjustMinutes(editSleepStartMinutes, 60);
      break;
    case TimeMenuAction::SleepStartMinuteDown:
      editSleepStartMinutes = adjustMinutes(editSleepStartMinutes, -5);
      break;
    case TimeMenuAction::SleepStartMinuteUp:
      editSleepStartMinutes = adjustMinutes(editSleepStartMinutes, 5);
      break;
    case TimeMenuAction::SleepEndHourDown:
      editSleepEndMinutes = adjustMinutes(editSleepEndMinutes, -60);
      break;
    case TimeMenuAction::SleepEndHourUp:
      editSleepEndMinutes = adjustMinutes(editSleepEndMinutes, 60);
      break;
    case TimeMenuAction::SleepEndMinuteDown:
      editSleepEndMinutes = adjustMinutes(editSleepEndMinutes, -5);
      break;
    case TimeMenuAction::SleepEndMinuteUp:
      editSleepEndMinutes = adjustMinutes(editSleepEndMinutes, 5);
      break;
    case TimeMenuAction::Cancel:
      closeConfigMenu();
      return;
    case TimeMenuAction::Save: {
      use24Hour = editUse24Hour;
      clockPlatformSaveUse24Hour(use24Hour);
      sleepModeEnabled = editSleepModeEnabled;
      sleepModeStartMinutes = editSleepStartMinutes;
      sleepModeEndMinutes = editSleepEndMinutes;
      clockPlatformSaveSleepSettings({sleepModeEnabled, sleepModeStartMinutes,
                                      sleepModeEndMinutes});
      sleepModeDismissed = false;

      if (editDateTimeChanged) {
        const unsigned long currentDays =
            getClockSeconds() / (24UL * 60UL * 60UL);
        clockBaseSeconds = currentDays * 24UL * 60UL * 60UL +
                           static_cast<unsigned long>(editHour) * 60UL * 60UL +
                           static_cast<unsigned long>(editMinute) * 60UL;
        clockBaseMillis = clockPlatformMillis();
        calendarMonth = editMonth;
        calendarDay = editDay;
        calendarWeekday = editWeekday;
        calendarYear = editYear;
        calendarBaseDay = currentDays;

        tm manualTime = {};
        manualTime.tm_year = editYear - 1900;
        manualTime.tm_mon = editMonth - 1;
        manualTime.tm_mday = editDay;
        manualTime.tm_wday = editWeekday;
        manualTime.tm_hour = editHour;
        manualTime.tm_min = editMinute;
        manualTime.tm_sec = 0;
        clockPlatformWriteRtc(manualTime);
      }
      refreshClock();
      closeConfigMenu();
      return;
    }
  }
  updateTimeEditor();
  updateSleepEditor();
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
  editDateTimeChanged = false;
  editSleepModeEnabled = sleepModeEnabled;
  editSleepStartMinutes = sleepModeStartMinutes;
  editSleepEndMinutes = sleepModeEndMinutes;
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
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 42, 54);

  timeDatePage = lv_obj_create(configMenu);
  lv_obj_remove_style_all(timeDatePage);
  lv_obj_set_size(timeDatePage, ScreenWidth, ScreenHeight);
  lv_obj_clear_flag(timeDatePage, LV_OBJ_FLAG_CLICKABLE);

  format12Button = createMenuButton(timeDatePage, "12 HOUR", 170, 54,
                                    TimeMenuAction::Format12);
  lv_obj_align(format12Button, LV_ALIGN_TOP_LEFT, 290, 88);
  format24Button = createMenuButton(timeDatePage, "24 HOUR", 170, 54,
                                    TimeMenuAction::Format24);
  lv_obj_align(format24Button, LV_ALIGN_TOP_RIGHT, -62, 88);

  lv_obj_t *panel = lv_obj_create(timeDatePage);
  lv_obj_remove_style_all(panel);
  lv_obj_set_size(panel, 720, 290);
  lv_obj_set_pos(panel, 272, 158);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *hourTitle = createLabel(panel, "HOUR", &lv_font_montserrat_14,
                                    0xB8BDC0);
  lv_obj_set_pos(hourTitle, 148, 28);
  lv_obj_t *minuteTitle = createLabel(panel, "MINUTE", &lv_font_montserrat_14,
                                      0xB8BDC0);
  lv_obj_set_pos(minuteTitle, 526, 28);

  lv_obj_t *hourDown = createMenuButton(panel, "-", 92, 70,
                                        TimeMenuAction::HourDown);
  lv_obj_set_pos(hourDown, 24, 78);
  editHourLabel = createLabel(panel, "00", &lv_font_montserrat_48, 0xF1F3F4);
  lv_obj_set_pos(editHourLabel, 132, 88);
  lv_obj_t *hourUp = createMenuButton(panel, "+", 92, 70,
                                      TimeMenuAction::HourUp);
  lv_obj_set_pos(hourUp, 214, 78);
  lv_obj_t *separator = createLabel(panel, ":", &lv_font_montserrat_48,
                                    0xB8BDC0);
  lv_obj_set_pos(separator, 350, 88);
  lv_obj_t *minuteDown = createMenuButton(panel, "-", 92, 70,
                                          TimeMenuAction::MinuteDown);
  lv_obj_set_pos(minuteDown, 408, 78);
  editMinuteLabel = createLabel(panel, "00", &lv_font_montserrat_48, 0xF1F3F4);
  lv_obj_set_pos(editMinuteLabel, 516, 88);
  lv_obj_t *minuteUp = createMenuButton(panel, "+", 92, 70,
                                        TimeMenuAction::MinuteUp);
  lv_obj_set_pos(minuteUp, 598, 78);
  editPeriodButton = createMenuButton(panel, "AM", 110, 50,
                                      TimeMenuAction::TogglePeriod);
  lv_obj_set_pos(editPeriodButton, 305, 18);
  editPeriodLabel = lv_obj_get_child(editPeriodButton, 0);

  lv_obj_t *monthButton = createMenuButton(panel, "MONTH 1", 150, 52,
                                           TimeMenuAction::MonthNext);
  lv_obj_set_pos(monthButton, 14, 216);
  editMonthLabel = lv_obj_get_child(monthButton, 0);
  lv_obj_t *dayButton = createMenuButton(panel, "DAY 01", 150, 52,
                                         TimeMenuAction::DayNext);
  lv_obj_set_pos(dayButton, 190, 216);
  editDayLabel = lv_obj_get_child(dayButton, 0);
  lv_obj_t *weekdayButton = createMenuButton(panel, "SUN", 150, 52,
                                             TimeMenuAction::WeekdayNext);
  lv_obj_set_pos(weekdayButton, 366, 216);
  editWeekdayLabel = lv_obj_get_child(weekdayButton, 0);
  lv_obj_t *yearButton = createMenuButton(panel, "2026", 150, 52,
                                          TimeMenuAction::YearNext);
  lv_obj_set_pos(yearButton, 542, 216);
  editYearLabel = lv_obj_get_child(yearButton, 0);

  screensaverPage = lv_obj_create(configMenu);
  lv_obj_remove_style_all(screensaverPage);
  lv_obj_set_size(screensaverPage, ScreenWidth, ScreenHeight);
  lv_obj_clear_flag(screensaverPage, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *sleepTitle = createLabel(screensaverPage, "SLEEP MODE",
                                     &lv_font_montserrat_20, 0xB8BDC0);
  lv_obj_align(sleepTitle, LV_ALIGN_TOP_LEFT, 300, 85);
  sleepModeButton = createMenuButton(screensaverPage, "OFF", 150, 54,
                                     TimeMenuAction::ToggleSleepMode);
  lv_obj_align(sleepModeButton, LV_ALIGN_TOP_LEFT, 470, 68);
  sleepModeLabel = lv_obj_get_child(sleepModeButton, 0);

  lv_obj_t *sleepPanel = lv_obj_create(screensaverPage);
  lv_obj_set_size(sleepPanel, 680, 250);
  lv_obj_align(sleepPanel, LV_ALIGN_TOP_RIGHT, -42, 160);
  lv_obj_set_style_bg_color(sleepPanel, lv_color_hex(0x25282B), 0);
  lv_obj_set_style_border_color(sleepPanel, lv_color_hex(0x555B60), 0);
  lv_obj_set_style_border_width(sleepPanel, 2, 0);
  lv_obj_set_style_radius(sleepPanel, 6, 0);
  lv_obj_clear_flag(sleepPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *startTitle = createLabel(sleepPanel, "START",
                                     &lv_font_montserrat_20, 0xB8BDC0);
  lv_obj_set_pos(startTitle, 22, 48);
  lv_obj_t *startHourDown = createMenuButton(
      sleepPanel, "H-", 62, 54, TimeMenuAction::SleepStartHourDown);
  lv_obj_set_pos(startHourDown, 116, 32);
  lv_obj_t *startMinuteDown = createMenuButton(
      sleepPanel, "M-", 62, 54, TimeMenuAction::SleepStartMinuteDown);
  lv_obj_set_pos(startMinuteDown, 190, 32);
  sleepStartLabel = createLabel(sleepPanel, "22:00", &lv_font_montserrat_32,
                                0xF1F3F4);
  lv_obj_set_pos(sleepStartLabel, 278, 42);
  lv_obj_t *startMinuteUp = createMenuButton(
      sleepPanel, "M+", 62, 54, TimeMenuAction::SleepStartMinuteUp);
  lv_obj_set_pos(startMinuteUp, 400, 32);
  lv_obj_t *startHourUp = createMenuButton(
      sleepPanel, "H+", 62, 54, TimeMenuAction::SleepStartHourUp);
  lv_obj_set_pos(startHourUp, 474, 32);

  lv_obj_t *endTitle = createLabel(sleepPanel, "END", &lv_font_montserrat_20,
                                   0xB8BDC0);
  lv_obj_set_pos(endTitle, 22, 153);
  lv_obj_t *endHourDown = createMenuButton(
      sleepPanel, "H-", 62, 54, TimeMenuAction::SleepEndHourDown);
  lv_obj_set_pos(endHourDown, 116, 137);
  lv_obj_t *endMinuteDown = createMenuButton(
      sleepPanel, "M-", 62, 54, TimeMenuAction::SleepEndMinuteDown);
  lv_obj_set_pos(endMinuteDown, 190, 137);
  sleepEndLabel = createLabel(sleepPanel, "07:00", &lv_font_montserrat_32,
                              0xF1F3F4);
  lv_obj_set_pos(sleepEndLabel, 278, 147);
  lv_obj_t *endMinuteUp = createMenuButton(
      sleepPanel, "M+", 62, 54, TimeMenuAction::SleepEndMinuteUp);
  lv_obj_set_pos(endMinuteUp, 400, 137);
  lv_obj_t *endHourUp = createMenuButton(
      sleepPanel, "H+", 62, 54, TimeMenuAction::SleepEndHourUp);
  lv_obj_set_pos(endHourUp, 474, 137);

  timeDateNavButton = createMenuButton(configMenu, "TIME & DATE", 190, 52,
                                       TimeMenuAction::ShowTimeDate);
  lv_obj_align(timeDateNavButton, LV_ALIGN_TOP_LEFT, 42, 106);
  screensaverNavButton = createMenuButton(
      configMenu, "SCREENSAVER", 190, 52, TimeMenuAction::ShowScreensaver);
  lv_obj_align(screensaverNavButton, LV_ALIGN_TOP_LEFT, 42, 168);

  lv_obj_t *navDivider = lv_obj_create(configMenu);
  lv_obj_remove_style_all(navDivider);
  lv_obj_set_pos(navDivider, 252, 128);
  lv_obj_set_size(navDivider, 2, 360);
  lv_obj_set_style_bg_color(navDivider, lv_color_hex(0x555B60), 0);
  lv_obj_set_style_bg_opa(navDivider, LV_OPA_COVER, 0);
  lv_obj_clear_flag(navDivider, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *cancel = createMenuButton(configMenu, "CANCEL", 170, 68,
                                      TimeMenuAction::Cancel);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 272, -44);
  lv_obj_t *save = createMenuButton(configMenu, "SAVE", 170, 68,
                                    TimeMenuAction::Save);
  lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, -32, -44);
  lv_obj_set_style_bg_color(save, lv_color_hex(0x287C8D), 0);
  updateTimeEditor();
  updateSleepEditor();
  showSettingsPage(false);
  lv_obj_clear_flag(configMenu, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

void createClockUi() {
  use24Hour = clockPlatformLoadUse24Hour();
  const int configuredStart = parseClockMinutes(ClockConfig::SleepModeStart);
  const int configuredEnd = parseClockMinutes(ClockConfig::SleepModeEnd);
  const ClockSleepSettings sleepDefaults = {
      ClockConfig::SleepModeEnabled,
      static_cast<uint16_t>(configuredStart >= 0 ? configuredStart : 22 * 60),
      static_cast<uint16_t>(configuredEnd >= 0 ? configuredEnd : 7 * 60),
  };
  const ClockSleepSettings savedSleepSettings =
      clockPlatformLoadSleepSettings(sleepDefaults);
  sleepModeEnabled = savedSleepSettings.enabled;
  sleepModeStartMinutes = savedSleepSettings.startMinutes;
  sleepModeEndMinutes = savedSleepSettings.endMinutes;

  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xB9C1A5), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(screen, onScreenPressed, LV_EVENT_PRESSED, nullptr);

  lcdPanel = lv_obj_create(screen);
  lv_obj_remove_style_all(lcdPanel);
  lv_obj_set_size(lcdPanel, ScreenWidth, ScreenHeight);
  lv_obj_align(lcdPanel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(lcdPanel, lv_color_hex(0xB9C1A5), 0);
  lv_obj_set_style_bg_opa(lcdPanel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(lcdPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(lcdPanel, LV_OBJ_FLAG_CLICKABLE);

  modeLabel = createLabel(lcdPanel, "AM", &lv_font_montserrat_48, 0x202821);
  lv_obj_align(modeLabel, LV_ALIGN_TOP_LEFT, 44, 112);
  modeBoldLabel =
      createLabel(lcdPanel, "AM", &lv_font_montserrat_48, 0x202821);
  lv_obj_align(modeBoldLabel, LV_ALIGN_TOP_LEFT, 46, 112);
  constexpr uint32_t SegmentColor = 0x202821;
  createWeekdaySegmentGlyph(weekdayGlyphs[0], lcdPanel, 347, 64,
                            SegmentColor);
  createWeekdaySegmentGlyph(weekdayGlyphs[1], lcdPanel, 415, 64,
                            SegmentColor);
  createSevenSegmentDigit(dateDigits[0], lcdPanel, 868, 64, 60, 104,
                          SevenSegmentProfile::Top, SegmentColor);
  createSevenSegmentDigit(dateDigits[1], lcdPanel, 936, 64, 60, 104,
                          SevenSegmentProfile::Top, SegmentColor);
  createSevenSegmentDigit(timeDigits[0], lcdPanel, 34, 215, 148, 342,
                          SevenSegmentProfile::Large, SegmentColor);
  createSevenSegmentDigit(timeDigits[1], lcdPanel, 184, 215, 148, 342,
                          SevenSegmentProfile::Large, SegmentColor);
  createLcdDot(lcdPanel, 356, 312, 34);
  createLcdDot(lcdPanel, 351, 430, 34);
  createSevenSegmentDigit(timeDigits[2], lcdPanel, 404, 215, 148, 342,
                          SevenSegmentProfile::Large, SegmentColor);
  createSevenSegmentDigit(timeDigits[3], lcdPanel, 554, 215, 148, 342,
                          SevenSegmentProfile::Large, SegmentColor);
  createSevenSegmentDigit(secondDigits[0], lcdPanel, 750, 336, 112, 221,
                          SevenSegmentProfile::Small, SegmentColor);
  createSevenSegmentDigit(secondDigits[1], lcdPanel, 870, 336, 112, 221,
                          SevenSegmentProfile::Small, SegmentColor);

  lv_obj_t *screensaverHotspot = lv_obj_create(screen);
  lv_obj_remove_style_all(screensaverHotspot);
  lv_obj_set_size(screensaverHotspot, 200, 200);
  lv_obj_align(screensaverHotspot, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(screensaverHotspot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(screensaverHotspot, openScreensaver, LV_EVENT_CLICKED,
                      nullptr);

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
  clockBaseMillis = clockPlatformMillis();
  calendarMonth = dateTime.tm_mon + 1;
  calendarDay = dateTime.tm_mday;
  calendarWeekday = dateTime.tm_wday;
  calendarYear = dateTime.tm_year + 1900;
  calendarBaseDay = currentDays;
  refreshClock();
}
