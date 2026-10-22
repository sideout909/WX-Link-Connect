#include "connect_wifi_scan.h"
#include <cassert>
#include <iostream>
int main() {
  wxconnect::WifiScanDriver driver;
  assert(driver.start(100));
  assert(!scan_blocking && last_config.ssid==nullptr && last_config.bssid==nullptr);
  assert(!last_config.show_hidden && last_config.channel==0);
  assert(last_config.home_chan_dwell_time==0 && last_config.scan_time.passive==0);
  assert(last_config.scan_time.active.min==100 && last_config.scan_time.active.max==120);
  assert(driver.poll(2501)==driver.kRunning); // Regression: old SDK failed at 2401 elapsed.
  assert(driver.poll(14999)==driver.kRunning);
  WiFi.complete(0,3);
  assert(driver.poll(15000)==3);
  driver.release(false);assert(stop_calls==0);
  assert(driver.start(20000));
  assert(driver.poll(34999)==driver.kRunning);
  assert(driver.poll(35000)==driver.kTimeout && driver.error()==ESP_ERR_TIMEOUT);
  driver.release(true);assert(stop_calls==1);
  WiFi.complete(0,9); // A late event outside an active attempt is ignored.
  assert(driver.start(40000));assert(driver.poll(40001)==driver.kRunning);
  WiFi.complete(1,0);assert(driver.poll(40002)==driver.kFailed);
  driver.release(true);
  start_error=12345;assert(!driver.start(50000));assert(driver.error()==12345);
  start_error=ESP_OK;assert(driver.start(UINT32_MAX-100));
  assert(driver.poll(100)==driver.kRunning);
  assert(driver.poll(14900)==driver.kTimeout);
  driver.release(true);assert(WiFi.registrations==1);
  std::cout<<"Wi-Fi scan driver lifecycle checks passed\n";
}
