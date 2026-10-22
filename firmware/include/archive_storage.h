#pragma once
#include <Arduino.h>
#include "history_archive.h"
namespace archive_storage {
constexpr uint16_t kPageSize=32;
struct State {
  bool ready=false;
  String message;
  uint16_t records=0;
  uint32_t bytes=0,free_bytes=0,write_failures=0,invalid_records=0;
  uint64_t oldest_ms=0,latest_ms=0,last_saved_ms=0;
};
bool begin(uint16_t station,char channel);
void loop(uint64_t now,bool current);
void observe(const weather::Sample& sample,weather::Result result,uint32_t rain_delta,uint64_t previous_rain,bool rain_continuous);
bool flush();
State state(uint64_t now);
uint32_t endInterval(uint64_t now);
const char* csvHeader();
String csvPage(uint32_t end,uint16_t offset);
bool selftest();
}
