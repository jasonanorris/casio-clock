#include "rtc_time.h"

#include <Arduino.h>
#include <driver/i2c.h>

#include "clock_ui.h"

namespace {
constexpr i2c_port_t I2cPort = I2C_NUM_0;
constexpr uint8_t RtcAddress = 0x51;
constexpr uint8_t TimeRegister = 0x04;
constexpr TickType_t I2cTimeout = pdMS_TO_TICKS(100);
constexpr int YearOffset = 1970;
bool available = false;

uint8_t toBcd(uint8_t value) { return (value / 10) * 16 + (value % 10); }
uint8_t fromBcd(uint8_t value) { return (value / 16) * 10 + (value % 16); }

bool validDateTime(const tm &value) {
  return value.tm_year + 1900 >= 2000 && value.tm_year + 1900 <= 2069 &&
         value.tm_mon >= 0 && value.tm_mon < 12 && value.tm_mday >= 1 &&
         value.tm_mday <= 31 && value.tm_wday >= 0 && value.tm_wday <= 6 &&
         value.tm_hour >= 0 && value.tm_hour <= 23 && value.tm_min >= 0 &&
         value.tm_min <= 59 && value.tm_sec >= 0 && value.tm_sec <= 59;
}
}  // namespace

bool readRtcDateTime(tm &dateTime) {
  uint8_t registers[7] = {};
  const esp_err_t result = i2c_master_write_read_device(
      I2cPort, RtcAddress, &TimeRegister, 1, registers, sizeof(registers),
      I2cTimeout);
  if (result != ESP_OK) {
    Serial.printf("PCF85063 read failed: %s\n", esp_err_to_name(result));
    return false;
  }

  if ((registers[0] & 0x80) != 0) {
    Serial.println("PCF85063 oscillator-stop flag is set; RTC time is invalid");
    return false;
  }

  dateTime = {};
  dateTime.tm_sec = fromBcd(registers[0] & 0x7F);
  dateTime.tm_min = fromBcd(registers[1] & 0x7F);
  dateTime.tm_hour = fromBcd(registers[2] & 0x3F);
  dateTime.tm_mday = fromBcd(registers[3] & 0x3F);
  dateTime.tm_wday = fromBcd(registers[4] & 0x07);
  dateTime.tm_mon = fromBcd(registers[5] & 0x1F) - 1;
  dateTime.tm_year = fromBcd(registers[6]) + YearOffset - 1900;
  return validDateTime(dateTime);
}

bool writeRtcDateTime(const tm &dateTime) {
  if (!validDateTime(dateTime)) {
    Serial.println("PCF85063 write rejected invalid date/time");
    return false;
  }

  const uint8_t data[] = {
      TimeRegister,
      toBcd(dateTime.tm_sec),
      toBcd(dateTime.tm_min),
      toBcd(dateTime.tm_hour),
      toBcd(dateTime.tm_mday),
      toBcd(dateTime.tm_wday),
      toBcd(dateTime.tm_mon + 1),
      toBcd(dateTime.tm_year + 1900 - YearOffset),
  };
  const esp_err_t result = i2c_master_write_to_device(
      I2cPort, RtcAddress, data, sizeof(data), I2cTimeout);
  if (result != ESP_OK) {
    Serial.printf("PCF85063 write failed: %s\n", esp_err_to_name(result));
    return false;
  }
  available = true;
  return true;
}

bool initRtcTime() {
  tm rtcTime = {};
  if (!readRtcDateTime(rtcTime)) {
    available = false;
    setClockUiTimeSource("MANUAL / OFFLINE");
    return false;
  }

  available = true;
  setClockUiDateTime(rtcTime);
  setClockUiTimeSource("RTC");
  Serial.printf("RTC loaded: %04d-%02d-%02d %02d:%02d:%02d\n",
                rtcTime.tm_year + 1900, rtcTime.tm_mon + 1, rtcTime.tm_mday,
                rtcTime.tm_hour, rtcTime.tm_min, rtcTime.tm_sec);
  return true;
}

bool rtcTimeAvailable() { return available; }
