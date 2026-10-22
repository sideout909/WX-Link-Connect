#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_TIMEOUT=263;
constexpr int WIFI_SCAN_TYPE_ACTIVE=0;
struct wifi_scan_config_t {
  uint8_t *ssid, *bssid;
  uint8_t channel;
  bool show_hidden;
  int scan_type;
  struct { struct {uint32_t min,max;} active; uint32_t passive; } scan_time;
  uint8_t home_chan_dwell_time;
};
inline wifi_scan_config_t last_config{};
inline int start_error=ESP_OK, stop_calls=0;
inline bool scan_blocking=false;
inline int esp_wifi_scan_start(const wifi_scan_config_t* config, bool block) {
  last_config=*config;scan_blocking=block;return start_error;
}
inline int esp_wifi_scan_stop() {++stop_calls;return ESP_OK;}
