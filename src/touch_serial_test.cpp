#include "touch_serial_test.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_display_panel.hpp>
#include <esp_io_expander.hpp>
#include <vector>

namespace {
using esp_panel::drivers::BusI2C;
using esp_panel::drivers::Touch;
using esp_panel::drivers::TouchGT911;
using esp_panel::drivers::TouchPoint;

constexpr int TouchWidth = 1024;
constexpr int TouchHeight = 600;
constexpr int I2cSda = 8;
constexpr int I2cScl = 9;
constexpr int I2cClockHz = 400 * 1000;
constexpr uint8_t Gt911Address = 0x5D;
constexpr uint8_t Ch422gAddress = 0x20;
constexpr int TouchInterruptGpio = 4;
constexpr uint8_t TouchResetExio = 1;
constexpr int MaxTouchPoints = 5;
constexpr int TouchPollIntervalMs = 50;

TouchGT911 *touch = nullptr;
unsigned long lastPollMs = 0;
bool wasTouching = false;
std::vector<TouchPoint> lastPoints;

bool initResetExpander(esp_expander::CH422G &expander) {
  Serial.println("Initializing CH422G for GT911 reset");

  if (!expander.init()) {
    Serial.println("CH422G init failed during touch setup");
    return false;
  }

  if (!expander.begin()) {
    Serial.println("CH422G begin failed during touch setup");
    return false;
  }

  if (!expander.enableAllIO_Output()) {
    Serial.println("CH422G output-mode setup failed during touch setup");
    return false;
  }

  return true;
}

bool resetGt911WithOfficialSequence() {
  Serial.println("Resetting GT911 using official Waveshare 5B sequence");

  esp_expander::CH422G expander(I2cScl, I2cSda, Ch422gAddress);
  if (!initResetExpander(expander)) {
    return false;
  }

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
  return true;
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
              .x_max = TouchWidth,
              .y_max = TouchHeight,
              .rst_gpio_num = -1,
              .int_gpio_num = TouchInterruptGpio,
              .levels_reset = 0,
              .levels_interrupt = 0,
          },
  };

  return new TouchGT911(busConfig, touchConfig);
}

bool pointsChanged(const std::vector<TouchPoint> &points) {
  if (points.size() != lastPoints.size()) {
    return true;
  }

  for (size_t i = 0; i < points.size(); ++i) {
    if (points[i] != lastPoints[i] ||
        points[i].strength != lastPoints[i].strength) {
      return true;
    }
  }

  return false;
}

void printPoints(const std::vector<TouchPoint> &points) {
  Serial.printf("touch points=%u", static_cast<unsigned>(points.size()));
  for (size_t i = 0; i < points.size(); ++i) {
    Serial.printf(" [%u] x=%d y=%d strength=%d", static_cast<unsigned>(i),
                  points[i].x, points[i].y, points[i].strength);
  }
  Serial.println();
}
}  // namespace

bool initTouchSerialTest() {
  Serial.println();
  Serial.println("Starting GT911 touch serial test.");
  Serial.println("Using official Waveshare 1024x600 5B touch pins");
  Serial.println("GT911 I2C: SDA=8 SCL=9 address=0x5D INT=GPIO4 RST=CH422G EXIO1");

  if (!resetGt911WithOfficialSequence()) {
    Serial.println("Touch test stopped because GT911 reset failed");
    return false;
  }

  touch = createTouch();
  if (touch == nullptr) {
    Serial.println("GT911 allocation failed");
    return false;
  }

  Serial.println("Initializing GT911 touch controller");
  if (!touch->init()) {
    Serial.println("GT911 init failed");
    return false;
  }

  if (!touch->begin()) {
    Serial.println("GT911 begin failed");
    return false;
  }

  Serial.println("GT911 touch ready. Touch the screen to print coordinates.");
  return true;
}

void pollTouchSerialTest() {
  if (touch == nullptr) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastPollMs < TouchPollIntervalMs) {
    return;
  }
  lastPollMs = now;

  if (!touch->readRawData(MaxTouchPoints, 0, 0)) {
    Serial.println("GT911 read failed");
    return;
  }

  std::vector<TouchPoint> points;
  if (!touch->getPoints(points)) {
    Serial.println("GT911 getPoints failed");
    return;
  }

  if (!points.empty()) {
    if (!wasTouching || pointsChanged(points)) {
      printPoints(points);
    }
    wasTouching = true;
    lastPoints = points;
    return;
  }

  if (wasTouching) {
    Serial.println("touch released");
    wasTouching = false;
    lastPoints.clear();
  }
}
