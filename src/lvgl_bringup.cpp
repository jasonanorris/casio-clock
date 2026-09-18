#include "lvgl_bringup.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_display_panel.hpp>
#include <esp_io_expander.hpp>
#include <lvgl.h>
#include <vector>

namespace {
using esp_panel::drivers::BusI2C;
using esp_panel::drivers::BusRGB;
using esp_panel::drivers::LCD;
using esp_panel::drivers::LCD_ST7262;
using esp_panel::drivers::Touch;
using esp_panel::drivers::TouchGT911;
using esp_panel::drivers::TouchPoint;

constexpr int ScreenWidth = 1024;
constexpr int ScreenHeight = 600;
constexpr int LcdColorBits = 16;
constexpr int RgbDataWidth = 16;
constexpr int RgbColorBits = 16;
constexpr int RgbPclkHz = 21 * 1000 * 1000;
constexpr int RgbHpw = 24;
constexpr int RgbHbp = 160;
constexpr int RgbHfp = 160;
constexpr int RgbVpw = 2;
constexpr int RgbVbp = 23;
constexpr int RgbVfp = 12;
constexpr int RgbBounceBufferSize = ScreenWidth * 10;

constexpr int RgbIoDisp = -1;
constexpr int RgbIoVsync = 3;
constexpr int RgbIoHsync = 46;
constexpr int RgbIoDe = 5;
constexpr int RgbIoPclk = 7;
constexpr int RgbIoData0 = 14;
constexpr int RgbIoData1 = 38;
constexpr int RgbIoData2 = 18;
constexpr int RgbIoData3 = 17;
constexpr int RgbIoData4 = 10;
constexpr int RgbIoData5 = 39;
constexpr int RgbIoData6 = 0;
constexpr int RgbIoData7 = 45;
constexpr int RgbIoData8 = 48;
constexpr int RgbIoData9 = 47;
constexpr int RgbIoData10 = 21;
constexpr int RgbIoData11 = 1;
constexpr int RgbIoData12 = 2;
constexpr int RgbIoData13 = 42;
constexpr int RgbIoData14 = 41;
constexpr int RgbIoData15 = 40;

constexpr int I2cSda = 8;
constexpr int I2cScl = 9;
constexpr int I2cClockHz = 400 * 1000;
constexpr uint8_t Ch422gAddress = 0x20;
constexpr uint8_t Gt911Address = 0x5D;
constexpr int TouchInterruptGpio = 4;
constexpr uint8_t TouchResetExio = 1;
constexpr uint8_t LcdBacklightExio = 2;
constexpr uint8_t LcdResetExio = 3;
constexpr int MaxTouchPoints = 5;
constexpr int DrawBufferRows = 40;

LCD *lcd = nullptr;
TouchGT911 *touch = nullptr;
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_indev_drv_t inputDriver;
lv_color_t *drawBufferPixels = nullptr;
lv_obj_t *statusLabel = nullptr;
lv_obj_t *timeLabel = nullptr;
lv_obj_t *dayLabel = nullptr;
lv_obj_t *dateLabel = nullptr;
lv_obj_t *modeLabel = nullptr;
lv_obj_t *lcdPanel = nullptr;
lv_obj_t *configMenu = nullptr;
lv_obj_t *editHourLabel = nullptr;
lv_obj_t *editMinuteLabel = nullptr;
lv_obj_t *editPeriodButton = nullptr;
lv_obj_t *editPeriodLabel = nullptr;
lv_obj_t *format12Button = nullptr;
lv_obj_t *format24Button = nullptr;
unsigned long lastLvglTickMs = 0;
unsigned long lastClockUpdateMs = 0;
unsigned long clockBaseMillis = 0;
unsigned long clockBaseSeconds = 0;
uint8_t editHour = 0;
uint8_t editMinute = 0;
bool illuminatorOn = false;
bool use24Hour = false;
bool editUse24Hour = false;

enum class TimeMenuAction : intptr_t {
  HourDown,
  HourUp,
  MinuteDown,
  MinuteUp,
  TogglePeriod,
  Format12,
  Format24,
  Cancel,
  Save,
};

bool initIoExpander(esp_expander::CH422G &expander) {
  Serial.println("Initializing CH422G for LCD/touch reset and backlight");

  if (!expander.init()) {
    Serial.println("CH422G init failed");
    return false;
  }

  if (!expander.begin()) {
    Serial.println("CH422G begin failed");
    return false;
  }

  if (!expander.enableAllIO_Output()) {
    Serial.println("CH422G output-mode setup failed");
    return false;
  }

  return true;
}

void resetLcd(esp_expander::CH422G &expander) {
  Serial.println("Resetting LCD via CH422G EXIO3");
  expander.digitalWrite(LcdResetExio, LOW);
  delay(10);
  expander.digitalWrite(LcdResetExio, HIGH);
  delay(100);
}

void resetTouch(esp_expander::CH422G &expander) {
  Serial.println("Resetting GT911 using official Waveshare 5B sequence");
  constexpr gpio_num_t touchInterrupt =
      static_cast<gpio_num_t>(TouchInterruptGpio);
  gpio_set_direction(touchInterrupt, GPIO_MODE_OUTPUT);
  gpio_set_level(touchInterrupt, LOW);
  delay(10);
  expander.digitalWrite(TouchResetExio, LOW);
  delay(100);
  expander.digitalWrite(TouchResetExio, HIGH);
  delay(200);
  gpio_reset_pin(touchInterrupt);
}

void setBacklight(esp_expander::CH422G &expander, bool on) {
  Serial.println(on ? "Turning LCD backlight on" : "Turning LCD backlight off");
  expander.digitalWrite(LcdBacklightExio, on ? HIGH : LOW);
}

LCD *createLcd() {
  BusRGB::Config busConfig = {
      .refresh_panel =
          BusRGB::RefreshPanelPartialConfig{
              .pclk_hz = RgbPclkHz,
              .h_res = ScreenWidth,
              .v_res = ScreenHeight,
              .hsync_pulse_width = RgbHpw,
              .hsync_back_porch = RgbHbp,
              .hsync_front_porch = RgbHfp,
              .vsync_pulse_width = RgbVpw,
              .vsync_back_porch = RgbVbp,
              .vsync_front_porch = RgbVfp,
              .data_width = RgbDataWidth,
              .bits_per_pixel = RgbColorBits,
              .bounce_buffer_size_px = RgbBounceBufferSize,
              .hsync_gpio_num = RgbIoHsync,
              .vsync_gpio_num = RgbIoVsync,
              .de_gpio_num = RgbIoDe,
              .pclk_gpio_num = RgbIoPclk,
              .disp_gpio_num = RgbIoDisp,
              .data_gpio_nums =
                  {
                      RgbIoData0, RgbIoData1, RgbIoData2, RgbIoData3,
                      RgbIoData4, RgbIoData5, RgbIoData6, RgbIoData7,
                      RgbIoData8, RgbIoData9, RgbIoData10, RgbIoData11,
                      RgbIoData12, RgbIoData13, RgbIoData14, RgbIoData15,
                  },
              .flags_pclk_active_neg = true,
          },
  };

  LCD::Config lcdConfig = {
      .device =
          LCD::DevicePartialConfig{
              .reset_gpio_num = -1,
              .bits_per_pixel = LcdColorBits,
          },
      .vendor =
          LCD::VendorPartialConfig{
              .hor_res = ScreenWidth,
              .ver_res = ScreenHeight,
          },
  };

  return new LCD_ST7262(busConfig, lcdConfig);
}

TouchGT911 *createTouch() {
  BusI2C::ControlPanelFullConfig i2cPanelConfig =
      ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG_WITH_ADDR(Gt911Address);

  BusI2C::Config busConfig = {
      .host_id = BusI2C::I2C_HOST_ID_DEFAULT,
      .host =
          BusI2C::HostPartialConfig{
              .sda_io_num = I2cSda,
              .scl_io_num = I2cScl,
              .sda_pullup_en = GPIO_PULLUP_ENABLE,
              .scl_pullup_en = GPIO_PULLUP_ENABLE,
              .clk_speed = I2cClockHz,
          },
      .control_panel = i2cPanelConfig,
  };

  Touch::Config touchConfig = {
      .device =
          Touch::DevicePartialConfig{
              .x_max = ScreenWidth,
              .y_max = ScreenHeight,
              .rst_gpio_num = -1,
              .int_gpio_num = TouchInterruptGpio,
              .levels_reset = 0,
              .levels_interrupt = 0,
          },
  };

  return new TouchGT911(busConfig, touchConfig);
}

bool initLcd() {
  Serial.println("Initializing ST7262 RGB LCD for LVGL");

  lcd = createLcd();
  if (lcd == nullptr) {
    Serial.println("LCD allocation failed");
    return false;
  }

  if (!lcd->init()) {
    Serial.println("LCD init failed");
    return false;
  }

  if (!lcd->begin()) {
    Serial.println("LCD begin failed");
    return false;
  }

  if (lcd->getBasicAttributes().basic_bus_spec.isFunctionValid(
          LCD::BasicBusSpecification::FUNC_DISPLAY_ON_OFF)) {
    lcd->setDisplayOnOff(true);
  }

  return true;
}

bool initTouch() {
  Serial.println("Initializing GT911 touch controller for LVGL");

  touch = createTouch();
  if (touch == nullptr) {
    Serial.println("GT911 allocation failed");
    return false;
  }

  if (!touch->init()) {
    Serial.println("GT911 init failed");
    return false;
  }

  if (!touch->begin()) {
    Serial.println("GT911 begin failed");
    return false;
  }

  return true;
}

void displayFlush(lv_disp_drv_t *disp, const lv_area_t *area,
                  lv_color_t *colorPixels) {
  (void)disp;

  const int width = area->x2 - area->x1 + 1;
  const int height = area->y2 - area->y1 + 1;
  if (lcd != nullptr) {
    lcd->drawBitmap(area->x1, area->y1, width, height,
                    reinterpret_cast<const uint8_t *>(colorPixels), -1);
  }

  lv_disp_flush_ready(disp);
}

void touchRead(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  (void)indev;

  static lv_point_t lastPoint = {0, 0};

  if (touch == nullptr || !touch->readRawData(MaxTouchPoints, 0, 0)) {
    data->state = LV_INDEV_STATE_REL;
    data->point = lastPoint;
    return;
  }

  std::vector<TouchPoint> points;
  if (!touch->getPoints(points) || points.empty()) {
    data->state = LV_INDEV_STATE_REL;
    data->point = lastPoint;
    return;
  }

  lastPoint.x = points[0].x;
  lastPoint.y = points[0].y;
  data->state = LV_INDEV_STATE_PR;
  data->point = lastPoint;
}

unsigned long getClockSeconds() {
  return clockBaseSeconds + ((millis() - clockBaseMillis) / 1000);
}

void updateClockLabel() {
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

  if (modeLabel != nullptr) {
    lv_label_set_text(modeLabel, use24Hour ? "24H" : "12H");
  }

  static constexpr const char *Weekdays[] = {"SUN", "MON", "TUE", "WED",
                                              "THU", "FRI", "SAT"};
  const unsigned long elapsedDays = totalSeconds / (24UL * 60UL * 60UL);
  if (dayLabel != nullptr) {
    lv_label_set_text(dayLabel, Weekdays[elapsedDays % 7]);
  }
  if (dateLabel != nullptr) {
    lv_label_set_text_fmt(dateLabel, "1-%02lu", (elapsedDays % 31) + 1);
  }
}

void onScreenPressed(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED || statusLabel == nullptr ||
      lcdPanel == nullptr) {
    return;
  }

  illuminatorOn = !illuminatorOn;
  lv_obj_set_style_bg_color(
      lcdPanel, lv_color_hex(illuminatorOn ? 0xDCECA8 : 0xA8B58A), 0);
  lv_label_set_text(statusLabel,
                    illuminatorOn ? "ILLUMINATOR ON" : "TOUCH FOR LIGHT");
}

lv_obj_t *createLabel(lv_obj_t *parent, const char *text,
                      const lv_font_t *font, uint32_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  return label;
}

void closeConfigMenu() {
  if (configMenu != nullptr) {
    lv_obj_del(configMenu);
    configMenu = nullptr;
    editHourLabel = nullptr;
    editMinuteLabel = nullptr;
    editPeriodButton = nullptr;
    editPeriodLabel = nullptr;
    format12Button = nullptr;
    format24Button = nullptr;
  }
}

void updateTimeEditor() {
  const uint8_t displayHour =
      editUse24Hour ? editHour : ((editHour % 12) == 0 ? 12 : editHour % 12);
  if (editHourLabel != nullptr) {
    lv_label_set_text_fmt(editHourLabel, "%02u",
                          static_cast<unsigned int>(displayHour));
  }
  if (editMinuteLabel != nullptr) {
    lv_label_set_text_fmt(editMinuteLabel, "%02u",
                          static_cast<unsigned int>(editMinute));
  }
  if (editPeriodLabel != nullptr) {
    lv_label_set_text(editPeriodLabel, editHour < 12 ? "AM" : "PM");
  }
  if (editPeriodButton != nullptr) {
    if (editUse24Hour) {
      lv_obj_add_flag(editPeriodButton, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(editPeriodButton, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (format12Button != nullptr && format24Button != nullptr) {
    lv_obj_set_style_bg_color(format12Button,
                              lv_color_hex(editUse24Hour ? 0x34383C : 0x287C8D),
                              0);
    lv_obj_set_style_bg_color(format24Button,
                              lv_color_hex(editUse24Hour ? 0x287C8D : 0x34383C),
                              0);
  }
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
      updateClockLabel();
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

  configMenu = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(configMenu);
  lv_obj_set_size(configMenu, ScreenWidth, ScreenHeight);
  lv_obj_align(configMenu, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(configMenu, lv_color_hex(0x17191B), 0);
  lv_obj_set_style_bg_opa(configMenu, LV_OPA_COVER, 0);
  lv_obj_add_flag(configMenu, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *title = createLabel(configMenu, "SETTINGS", &lv_font_montserrat_32,
                                0xF1F3F4);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 72, 30);

  lv_obj_t *section = createLabel(configMenu, "TIME FORMAT", &lv_font_montserrat_20,
                                  0x48B8D0);
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
  lv_obj_align(editPeriodButton, LV_ALIGN_BOTTOM_MID, 0, -12);
  editPeriodLabel = lv_obj_get_child(editPeriodButton, 0);

  lv_obj_t *cancel = createMenuButton(configMenu, "CANCEL", 170, 68,
                                      TimeMenuAction::Cancel);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 132, -42);
  lv_obj_t *save = createMenuButton(configMenu, "SAVE", 170, 68,
                                    TimeMenuAction::Save);
  lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, -132, -42);
  lv_obj_set_style_bg_color(save, lv_color_hex(0x287C8D), 0);

  updateTimeEditor();
}

void createLvglUi() {
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

  lv_obj_t *waterResist = createLabel(bezel, "WATER RESIST", &lv_font_montserrat_14,
                                      0x48B8D0);
  lv_obj_align(waterResist, LV_ALIGN_TOP_LEFT, 86, 63);

  lv_obj_t *alarm = createLabel(bezel, "ALARM  CHRONOGRAPH", &lv_font_montserrat_14,
                                0xD86A62);
  lv_obj_align(alarm, LV_ALIGN_TOP_RIGHT, -86, 63);

  lv_obj_t *accent = lv_obj_create(bezel);
  lv_obj_remove_style_all(accent);
  lv_obj_set_size(accent, 680, 4);
  lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_set_style_bg_color(accent, lv_color_hex(0xB94745), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  lv_obj_clear_flag(accent, LV_OBJ_FLAG_CLICKABLE);

  lcdPanel = lv_obj_create(bezel);
  lv_obj_remove_style_all(lcdPanel);
  lv_obj_set_size(lcdPanel, 710, 280);
  lv_obj_align(lcdPanel, LV_ALIGN_CENTER, 0, 18);
  lv_obj_set_style_bg_color(lcdPanel, lv_color_hex(0xA8B58A), 0);
  lv_obj_set_style_bg_opa(lcdPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(lcdPanel, lv_color_hex(0x111512), 0);
  lv_obj_set_style_border_width(lcdPanel, 12, 0);
  lv_obj_set_style_radius(lcdPanel, 5, 0);
  lv_obj_clear_flag(lcdPanel, LV_OBJ_FLAG_CLICKABLE);

  dayLabel = createLabel(lcdPanel, "SUN", &lv_font_montserrat_20, 0x20271D);
  lv_obj_align(dayLabel, LV_ALIGN_TOP_LEFT, 34, 28);

  dateLabel = createLabel(lcdPanel, "1-01", &lv_font_montserrat_20, 0x20271D);
  lv_obj_align(dateLabel, LV_ALIGN_TOP_RIGHT, -34, 28);

  modeLabel = createLabel(lcdPanel, "12H", &lv_font_montserrat_14, 0x20271D);
  lv_obj_align(modeLabel, LV_ALIGN_TOP_MID, 0, 32);

  timeLabel = createLabel(lcdPanel, "00:00:00", &lv_font_montserrat_48,
                          0x151B14);
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, 24);
  updateClockLabel();

  lv_obj_t *timebase = createLabel(lcdPanel, "BOOT TIME", &lv_font_montserrat_14,
                                   0x30382B);
  lv_obj_align(timebase, LV_ALIGN_BOTTOM_MID, 0, -24);

  statusLabel = createLabel(bezel, "TOUCH FOR LIGHT", &lv_font_montserrat_14,
                            0xD2D5D6);
  lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -25);

  lv_obj_t *settingsHotspot = lv_obj_create(screen);
  lv_obj_remove_style_all(settingsHotspot);
  lv_obj_set_size(settingsHotspot, 200, 200);
  lv_obj_align(settingsHotspot, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_flag(settingsHotspot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(settingsHotspot, openConfigMenu, LV_EVENT_CLICKED, nullptr);
}

bool initLvgl() {
  Serial.println("Initializing LVGL 8.4.0");

  drawBufferPixels = static_cast<lv_color_t *>(heap_caps_malloc(
      ScreenWidth * DrawBufferRows * sizeof(lv_color_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (drawBufferPixels == nullptr) {
    Serial.println("LVGL draw buffer allocation failed");
    return false;
  }

  lv_init();
  lv_disp_draw_buf_init(&drawBuffer, drawBufferPixels, nullptr,
                        ScreenWidth * DrawBufferRows);

  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = ScreenWidth;
  displayDriver.ver_res = ScreenHeight;
  displayDriver.flush_cb = displayFlush;
  displayDriver.draw_buf = &drawBuffer;
  lv_disp_drv_register(&displayDriver);

  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = touchRead;
  lv_indev_drv_register(&inputDriver);

  createLvglUi();
  lastLvglTickMs = millis();
  return true;
}
}  // namespace

bool initLvglBringup() {
  Serial.println();
  Serial.println("Starting LVGL bring-up for Waveshare ESP32-S3-Touch-LCD-5B");
  Serial.println("Using official 1024x600 5B RGB LCD and GT911 touch data");

  {
    esp_expander::CH422G expander(I2cScl, I2cSda, Ch422gAddress);
    if (!initIoExpander(expander)) {
      Serial.println("LVGL bring-up stopped because CH422G setup failed");
      return false;
    }

    resetLcd(expander);
    resetTouch(expander);

    if (!initLcd()) {
      setBacklight(expander, false);
      return false;
    }

    setBacklight(expander, true);
  }

  if (!initTouch()) {
    return false;
  }

  if (!initLvgl()) {
    return false;
  }

  Serial.println("LVGL bring-up ready.");
  return true;
}

void runLvglBringupLoop() {
  if (drawBufferPixels == nullptr) {
    return;
  }

  const unsigned long now = millis();
  if (now != lastLvglTickMs) {
    lv_tick_inc(now - lastLvglTickMs);
    lastLvglTickMs = now;
  }

  if (now - lastClockUpdateMs >= 1000) {
    lastClockUpdateMs = now;
    updateClockLabel();
  }

  lv_timer_handler();
}
