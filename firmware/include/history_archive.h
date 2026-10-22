#pragma once
#include "weather_store.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>

namespace history_archive {
constexpr uint64_t kIntervalMs=300000;
constexpr uint32_t kRecords=7*24*12;
constexpr uint8_t kPartial=1,kRainPartial=2,kSaturated=4;
// Fixed, little-endian on-device format, versioned by its file name and CRC seed.
// Never include credentials, callsigns, coordinates or raw packets here.
struct Record {
  uint32_t interval=0;
  uint16_t station=0;
  uint8_t channel=0,flags=0;
  float temperature_mean=0,temperature_min=0,temperature_max=0;
  uint32_t lux_mean=0,lux_peak=0;
  float wind_mean=0,wind_peak=0;
  uint16_t humidity_tenths=0,rain_tips=0;
  uint16_t temperature_count=0,humidity_count=0,lux_count=0,wind_count=0;
  uint16_t accepted=0,repeats=0,rejected=0;
  uint16_t current_seconds=0,observed_seconds=0,rain_samples=0;
  uint32_t crc=0;
};
static_assert(sizeof(Record)==64,"Archive record layout must remain 64 bytes");
static_assert(offsetof(Record,crc)==60,"Archive CRC covers the first 60 bytes");
inline uint32_t checksum(const Record& r){
  uint32_t crc=0x41524331;const auto* bytes=reinterpret_cast<const uint8_t*>(&r);
  for(size_t n=0;n<offsetof(Record,crc);++n){crc^=bytes[n];for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}
  return ~crc;
}
inline bool valid(const Record& r,uint16_t station,char channel){
  return r.interval&&r.station==station&&r.channel==uint8_t(channel)&&r.crc==checksum(r)&&
    r.observed_seconds<=300&&r.current_seconds<=r.observed_seconds&&
    std::isfinite(r.temperature_mean)&&std::isfinite(r.temperature_min)&&std::isfinite(r.temperature_max)&&
    std::isfinite(r.wind_mean)&&std::isfinite(r.wind_peak);
}
inline bool inWindow(uint32_t interval,uint32_t end){return interval<=end&&end-interval<kRecords;}

class Accumulator {
 public:
  explicit Accumulator(uint16_t station=679,char channel='A'):station_(station),channel_(channel){}
  std::optional<Record> advance(uint64_t now,bool uncertain_previous_rain=false){
    const uint32_t next=now/kIntervalMs;
    if(!next|| (active_&&next<record_.interval))return {};
    if(active_&&next==record_.interval)return {};
    std::optional<Record> completed;
    if(active_){
      // Normal loop jitter can put the next tick just over the boundary.
      // Account for that final second, but never bridge an actual long gap.
      const uint64_t end_second=(uint64_t(record_.interval)+1)*300;
      if(last_second_!=UINT64_MAX&&end_second>last_second_&&end_second-last_second_<=2&&now<end_second*1000+2000){
        const uint16_t tail=end_second-last_second_-1;
        record_.observed_seconds=std::min(unsigned(300),unsigned(record_.observed_seconds+tail));
        if(last_current_)record_.current_seconds=std::min(unsigned(record_.observed_seconds),unsigned(record_.current_seconds+tail));
      }
      if(uncertain_previous_rain)rain_uncertain_=true;
      completed=snapshot();
    }
    record_=Record{};record_.interval=next;record_.station=station_;record_.channel=channel_;
    temperature_sum_=wind_sum_=0;lux_sum_=humidity_sum_=0;
    last_second_=UINT64_MAX;last_current_=false;rain_first_=rain_last_=0;rain_uncertain_=false;active_=true;
    return completed;
  }
  std::optional<Record> tick(uint64_t now,bool current){
    auto completed=advance(now);
    if(!active_||now/kIntervalMs!=record_.interval)return completed;
    const uint64_t second=now/1000;
    if(last_second_==UINT64_MAX||second>last_second_){
      const uint64_t elapsed=last_second_==UINT64_MAX?1:second-last_second_;
      const uint16_t span=elapsed<=2?elapsed:1;
      record_.observed_seconds=std::min(unsigned(300),unsigned(record_.observed_seconds+span));
      if(current)record_.current_seconds=std::min(unsigned(record_.observed_seconds),unsigned(record_.current_seconds+(last_current_?span:1)));
      last_second_=second;last_current_=current;
    }
    return completed;
  }
  std::optional<Record> observe(const weather::Sample& s,weather::Result result,uint32_t rain_delta,uint64_t previous_rain,bool rain_continuous){
    if(s.station!=station_||s.channel!=char(channel_))return {};
    const bool rain_crossing=result==weather::Result::Accepted&&s.rain_tips.valid&&rain_delta&&previous_rain<s.time_ms/kIntervalMs*kIntervalMs;
    auto completed=advance(s.time_ms,rain_crossing);
    if(!active_||s.time_ms/kIntervalMs!=record_.interval)return completed;
    if(result==weather::Result::Duplicate){increment(record_.repeats);return completed;}
    if(result!=weather::Result::Accepted){increment(record_.rejected);return completed;}
    increment(record_.accepted);
    if(s.temperature.valid&&increment(record_.temperature_count)){
      if(record_.temperature_count==1)record_.temperature_min=record_.temperature_max=s.temperature.value;
      temperature_sum_+=s.temperature.value;
      record_.temperature_min=std::min(record_.temperature_min,s.temperature.value);record_.temperature_max=std::max(record_.temperature_max,s.temperature.value);
    }
    if(s.humidity.valid&&increment(record_.humidity_count))humidity_sum_+=s.humidity.value;
    if(s.lux.valid&&increment(record_.lux_count)){lux_sum_+=s.lux.value;record_.lux_peak=std::max(record_.lux_peak,s.lux.value);}
    if(s.wind.valid&&increment(record_.wind_count)){wind_sum_+=s.wind.value;record_.wind_peak=std::max(record_.wind_peak,s.wind.value);}
    if(s.rain_tips.valid){
      increment(record_.rain_samples);
      if(!rain_first_){rain_first_=s.time_ms;if(previous_rain>uint64_t(record_.interval)*kIntervalMs)rain_uncertain_=true;}
      rain_last_=s.time_ms;rain_uncertain_|=!rain_continuous||rain_crossing;
      if(rain_delta>uint32_t(UINT16_MAX-record_.rain_tips)){record_.rain_tips=UINT16_MAX;record_.flags|=kSaturated;rain_uncertain_=true;}
      else record_.rain_tips+=rain_delta;
    }
    return completed;
  }
  Record snapshot()const{
    Record r=record_;
    if(r.temperature_count)r.temperature_mean=temperature_sum_/r.temperature_count;
    if(r.wind_count)r.wind_mean=wind_sum_/r.wind_count;
    if(r.lux_count)r.lux_mean=uint32_t((lux_sum_+r.lux_count/2)/r.lux_count);
    if(r.humidity_count)r.humidity_tenths=uint16_t((humidity_sum_*10+r.humidity_count/2)/r.humidity_count);
    if(r.observed_seconds<300)r.flags|=kPartial;
    const uint64_t end=(uint64_t(r.interval)+1)*kIntervalMs;
    if(!rain_first_||rain_uncertain_||end<rain_last_||end-rain_last_>=weather::kStaleMs||r.observed_seconds<300)r.flags|=kRainPartial;
    r.crc=checksum(r);return r;
  }
  // On a controlled restart, retain the already recorded part of this interval.
  // A restart leaves coverage partial, and rain continuity must be re-established.
  bool resume(const Record& r){
    if(!valid(r,station_,channel_))return false;
    advance(uint64_t(r.interval)*kIntervalMs);record_=r;record_.flags|=kPartial|kRainPartial;
    temperature_sum_=double(r.temperature_mean)*r.temperature_count;wind_sum_=double(r.wind_mean)*r.wind_count;
    lux_sum_=uint64_t(r.lux_mean)*r.lux_count;humidity_sum_=uint64_t(r.humidity_tenths)*r.humidity_count/10;
    rain_uncertain_=true;return true;
  }
  bool active()const{return active_;}
 private:
  bool increment(uint16_t& value){if(value==UINT16_MAX){record_.flags|=kSaturated;return false;}++value;return true;}
  uint16_t station_;char channel_;
  bool active_=false,rain_uncertain_=false,last_current_=false;
  Record record_{};
  double temperature_sum_=0,wind_sum_=0;
  uint64_t lux_sum_=0,humidity_sum_=0,last_second_=UINT64_MAX,rain_first_=0,rain_last_=0;
};
}
