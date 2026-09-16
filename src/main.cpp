#include <Arduino.h>
#include <Esp.h>
#include <esp_chip_info.h>

#include "hardware_config.h"

namespace {
uint32_t heartbeatCount = 0;

void printBytes(const char *label, uint32_t bytes) {
  Serial.printf("%s: %lu bytes (%.2f MB)\n", label,
                static_cast<unsigned long>(bytes),
                static_cast<double>(bytes) / (1024.0 * 1024.0));
}

void printChipInfo() {
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo);

  Serial.println();
  Serial.println("Chip information:");
  Serial.printf("  Model: ESP32-S3\n");
  Serial.printf("  Revision: %u\n", chipInfo.revision);
  Serial.printf("  Cores: %u\n", chipInfo.cores);
  Serial.printf("  Features:%s%s%s%s\n",
                (chipInfo.features & CHIP_FEATURE_WIFI_BGN) ? " WiFi" : "",
                (chipInfo.features & CHIP_FEATURE_BLE) ? " BLE" : "",
                (chipInfo.features & CHIP_FEATURE_BT) ? " BT" : "",
                (chipInfo.features & CHIP_FEATURE_EMB_FLASH) ? " embedded-flash"
                                                             : "");
}

void printMemoryInfo() {
  Serial.println();
  Serial.println("Memory information:");
  printBytes("  Flash size", ESP.getFlashChipSize());
  printBytes("  Expected flash", HardwareConfig::ExpectedFlashBytes);
  printBytes("  PSRAM size", ESP.getPsramSize());
  printBytes("  Expected PSRAM", HardwareConfig::ExpectedPsramBytes);
  printBytes("  Free heap", ESP.getFreeHeap());
  printBytes("  Minimum free heap", ESP.getMinFreeHeap());
  printBytes("  Free PSRAM", ESP.getFreePsram());
}

void printStartupBanner() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(" casio-clock ESP32-S3 serial sanity test");
  Serial.println("========================================");
  Serial.printf("Project: %s\n", HardwareConfig::ProjectName);
  Serial.printf("Board: %s\n", HardwareConfig::BoardName);
  Serial.printf("Variant: %s\n", HardwareConfig::BoardVariant);
  Serial.printf("Serial: %lu baud on native USB CDC\n",
                HardwareConfig::SerialBaud);

  printChipInfo();
  printMemoryInfo();

  Serial.println();
  Serial.println("Heartbeat starting.");
}
}  // namespace

void setup() {
  Serial.begin(HardwareConfig::SerialBaud);
  delay(1500);
  printStartupBanner();
}

void loop() {
  ++heartbeatCount;
  Serial.printf("heartbeat=%lu uptime_ms=%lu free_heap=%lu free_psram=%lu\n",
                static_cast<unsigned long>(heartbeatCount),
                static_cast<unsigned long>(millis()),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getFreePsram()));
  delay(1000);
}
