#pragma once
#include "weather_store.h"
#include "daily_rain.h"
namespace checkpoint {
bool begin();
bool selftest();
bool restore(weather::Bucket* buckets, weather::Store& store, weather::DailyRain& rain,uint64_t now,uint16_t station,char channel,const char* path="/connect/weather.bin");
bool save(const weather::Bucket* buckets,const weather::Store& store,const weather::DailyRain& rain,uint64_t now,uint16_t station,char channel,const char* path="/connect/weather.bin");
}
