#include "lvgl_bringup.h"
#include "clock_ui.h"

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
constexpr int PartialDrawBufferRows = 40;

LCD *lcd = nullptr;
TouchGT911 *touch = nullptr;
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_indev_drv_t inputDriver;
lv_color_t *drawBufferPixels = nullptr;
size_t drawBufferPixelCount = 0;
unsigned long lastLvglTickMs = 0;
unsigned long lastClockUpdateMs = 0;

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

bool initLvgl() {
  Serial.println("Initializing LVGL 8.4.0");

  drawBufferPixelCount = ScreenWidth * ScreenHeight;
  drawBufferPixels = static_cast<lv_color_t *>(heap_caps_malloc(
      drawBufferPixelCount * sizeof(lv_color_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (drawBufferPixels == nullptr) {
    Serial.println("Full-screen LVGL buffer unavailable; using partial buffer");
    drawBufferPixelCount = ScreenWidth * PartialDrawBufferRows;
    drawBufferPixels = static_cast<lv_color_t *>(heap_caps_malloc(
        drawBufferPixelCount * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (drawBufferPixels == nullptr) {
      Serial.println("LVGL draw buffer allocation failed");
      return false;
    }
  } else {
    Serial.println("Using full-screen LVGL draw buffer in PSRAM");
  }

  lv_init();
  lv_disp_draw_buf_init(&drawBuffer, drawBufferPixels, nullptr,
                        drawBufferPixelCount);

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

  createClockUi();
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
    updateClockUi();
  }

  lv_timer_handler();
}
