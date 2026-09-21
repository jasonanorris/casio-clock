#include "network_time.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "clock_config.h"
#include "clock_ui.h"
#include "rtc_time.h"

#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#else
#include "wifi_credentials.h.example"
#endif

namespace {
constexpr char NtpServer1[] = "pool.ntp.org";
constexpr char NtpServer2[] = "time.nist.gov";
constexpr unsigned long ConnectTimeoutMs = 15000;
constexpr unsigned long RetryIntervalMs = 60000;
constexpr unsigned long SyncIntervalMs = 6UL * 60UL * 60UL * 1000UL;

unsigned long connectionStartedMs = 0;
unsigned long lastConnectionAttemptMs = 0;
unsigned long lastSyncMs = 0;
unsigned long lastSyncAttemptMs = 0;
bool connecting = false;
bool timeConfigured = false;

bool credentialsConfigured() { return WifiCredentials::Ssid[0] != '\0'; }

void startConnection() {
  Serial.printf("Connecting to Wi-Fi SSID: %s\n", WifiCredentials::Ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WifiCredentials::Ssid, WifiCredentials::Password);
  connectionStartedMs = millis();
  lastConnectionAttemptMs = connectionStartedMs;
  connecting = true;
}

void syncTime() {
  lastSyncAttemptMs = millis();
  tm localTime = {};
  if (!getLocalTime(&localTime, 10)) {
    return;
  }

  setClockUiDateTime(localTime);
  if (writeRtcDateTime(localTime)) {
    Serial.println("RTC updated from NTP");
  }
  lastSyncMs = millis();
  Serial.printf("NTP synchronized: %04d-%02d-%02d %02d:%02d:%02d\n",
                localTime.tm_year + 1900, localTime.tm_mon + 1,
                localTime.tm_mday, localTime.tm_hour, localTime.tm_min,
                localTime.tm_sec);
}
}  // namespace

void initNetworkTime() {
  if (!credentialsConfigured()) {
    Serial.println("Wi-Fi disabled: include/wifi_credentials.h is empty");
    return;
  }
  startConnection();
}

void runNetworkTimeLoop() {
  if (!credentialsConfigured()) {
    return;
  }

  const unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (connecting) {
      connecting = false;
      Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    }
    if (!timeConfigured) {
      configTzTime(ClockConfig::Timezone, NtpServer1, NtpServer2);
      timeConfigured = true;
    }
    if ((lastSyncMs == 0 && now - lastSyncAttemptMs >= 1000) ||
        (lastSyncMs != 0 && now - lastSyncMs >= SyncIntervalMs)) {
      syncTime();
    }
    return;
  }

  if (connecting && now - connectionStartedMs >= ConnectTimeoutMs) {
    connecting = false;
    WiFi.disconnect();
    Serial.println("Wi-Fi connection timed out; clock remains offline");
  }
  if (!connecting && now - lastConnectionAttemptMs >= RetryIntervalMs) {
    startConnection();
  }
}
