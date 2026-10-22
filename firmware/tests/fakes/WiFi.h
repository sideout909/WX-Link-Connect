#pragma once
#include <functional>
#include <cstdint>
using WiFiEvent_t = int;
struct WiFiEventInfo_t { struct {uint32_t status;} wifi_scan_done; };
constexpr int ARDUINO_EVENT_WIFI_SCAN_DONE=1;
struct FakeWiFi {
  std::function<void(WiFiEvent_t,WiFiEventInfo_t)> callback;
  int results=0, deletes=0, registrations=0;
  void onEvent(std::function<void(WiFiEvent_t,WiFiEventInfo_t)> cb, int) { callback=cb;++registrations; }
  void scanDelete(){++deletes;}
  int scanComplete(){return results;}
  void complete(uint32_t status,int count){results=count;callback(1,{{status}});}
};
inline FakeWiFi WiFi;
