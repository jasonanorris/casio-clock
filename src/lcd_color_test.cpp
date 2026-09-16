#include "lcd_color_test.h"

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <esp_io_expander.hpp>

namespace {
using esp_panel::drivers::BusRGB;
using esp_panel::drivers::LCD;
using esp_panel::drivers::LCD_ST7262;

constexpr int LcdWidth = 1024;
constexpr int LcdHeight = 600;
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
constexpr int RgbBounceBufferSize = LcdWidth * 10;

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
constexpr uint8_t Ch422gAddress = 0x20;
constexpr uint8_t TouchResetExio = 1;
constexpr uint8_t LcdBacklightExio = 2;
constexpr uint8_t LcdResetExio = 3;

bool initIoExpander(esp_expander::CH422G &expander) {
  Serial.println("Initializing CH422G IO expander");

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

  expander.digitalWrite(TouchResetExio, HIGH);
  expander.digitalWrite(LcdBacklightExio, LOW);
  return true;
}

void resetLcdWithExpander(esp_expander::CH422G &expander) {
  Serial.println("Resetting LCD via CH422G EXIO3");
  expander.digitalWrite(LcdResetExio, LOW);
  delay(10);
  expander.digitalWrite(LcdResetExio, HIGH);
  delay(100);
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
              .h_res = LcdWidth,
              .v_res = LcdHeight,
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
              .hor_res = LcdWidth,
              .ver_res = LcdHeight,
          },
  };

  return new LCD_ST7262(busConfig, lcdConfig);
}
}  // namespace

void runLcdColorTest() {
  Serial.println("LCD color-bar test for Waveshare ESP32-S3-Touch-LCD-5B");
  Serial.println("Using official 1024x600 5B RGB timing and CH422G pins");

  esp_expander::CH422G expander(I2cScl, I2cSda, Ch422gAddress);
  if (!initIoExpander(expander)) {
    Serial.println("LCD test stopped because CH422G setup failed");
    return;
  }

  resetLcdWithExpander(expander);

  Serial.println("Initializing ST7262 RGB LCD");
  LCD *lcd = createLcd();
  if (lcd == nullptr) {
    Serial.println("LCD allocation failed");
    return;
  }

  if (!lcd->init()) {
    Serial.println("LCD init failed");
    setBacklight(expander, false);
    return;
  }

  if (!lcd->begin()) {
    Serial.println("LCD begin failed");
    setBacklight(expander, false);
    return;
  }

  if (lcd->getBasicAttributes().basic_bus_spec.isFunctionValid(
          LCD::BasicBusSpecification::FUNC_DISPLAY_ON_OFF)) {
    lcd->setDisplayOnOff(true);
  }

  setBacklight(expander, true);

  Serial.println("Drawing color bar from top left to bottom right, order B-G-R");
  if (!lcd->colorBarTest()) {
    Serial.println("LCD color bar draw failed");
  } else {
    Serial.println("LCD color bar draw complete");
  }
}
