#pragma once

#include <WiFi.h>
#include <esp_wifi.h>
#include <atomic>

namespace wxconnect {
// Arduino 2.0.17's scanNetworks leaves home_chan_dwell_time uninitialized and
// derives a short global timeout from channel dwell. Start with an initialized
// IDF config instead. Arduino still owns result allocation in its SCAN_DONE
// handler, which runs before our event callback; only the loop reads results.
class WifiScanDriver {
 public:
  static constexpr int kRunning = -1, kFailed = -2, kTimeout = -3;
  static constexpr uint32_t kTimeoutMs = 15000;
  bool start(uint32_t now) {
    if (!registered_) {
      WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t info) {
        if (awaiting_.load(std::memory_order_acquire))
          completion_.store(uint32_t(info.wifi_scan_done.status) + 1,
                            std::memory_order_release);
      }, ARDUINO_EVENT_WIFI_SCAN_DONE);
      registered_ = true;
    }
    WiFi.scanDelete();
    completion_.store(0, std::memory_order_release);
    awaiting_.store(true, std::memory_order_release);
    wifi_scan_config_t config{};
    config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    config.scan_time.active.min = 100;
    config.scan_time.active.max = 120;
    // Zero selects the driver's default home-channel dwell.
    config.home_chan_dwell_time = 0;
    started_at_ = now;
    error_ = esp_wifi_scan_start(&config, false);
    if (error_ != ESP_OK) awaiting_.store(false, std::memory_order_release);
    return error_ == ESP_OK;
  }
  int poll(uint32_t now) {
    const uint32_t completed = completion_.load(std::memory_order_acquire);
    if (completed) {
      awaiting_.store(false, std::memory_order_release);
      if (completed != 1) { error_ = ESP_FAIL; return kFailed; }
      return WiFi.scanComplete();
    }
    if (uint32_t(now - started_at_) < kTimeoutMs) return kRunning;
    error_ = ESP_ERR_TIMEOUT;
    return kTimeout;
  }
  void release(bool stop) {
    awaiting_.store(false, std::memory_order_release);
    if (stop) esp_wifi_scan_stop();
    WiFi.scanDelete();
  }
  esp_err_t error() const { return error_; }
 private:
  bool registered_ = false;
  std::atomic<bool> awaiting_{false};
  std::atomic<uint32_t> completion_{0};
  uint32_t started_at_ = 0;
  esp_err_t error_ = ESP_OK;
};
}  // namespace wxconnect
