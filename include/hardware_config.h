#pragma once

namespace HardwareConfig {
constexpr const char *ProjectName = "casio-clock";
constexpr const char *BoardName = "Waveshare ESP32-S3-Touch-LCD-5B";
constexpr const char *BoardVariant = "1024x600 capacitive touch";
constexpr const char *UsbSerialPort = "/dev/ttyACM0";

constexpr unsigned long SerialBaud = 115200;
constexpr uint32_t ExpectedFlashBytes = 16UL * 1024UL * 1024UL;
constexpr uint32_t ExpectedPsramBytes = 8UL * 1024UL * 1024UL;
}  // namespace HardwareConfig
