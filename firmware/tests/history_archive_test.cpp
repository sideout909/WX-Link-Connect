#include "history_archive.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace history_archive;
int main(){
  constexpr uint64_t start=1700000100000ULL;
  static_assert(start%kIntervalMs==0);
  Accumulator a;
  for(unsigned second=0;second<300;++second){
    const auto now=start+second*1000;assert(!a.tick(now,second<240));
    if(second==10||second==20){
      weather::Sample s;s.time_ms=now;s.temperature.valid=s.humidity.valid=s.lux.valid=s.wind.valid=true;
      s.temperature.value=second==10?10:20;s.humidity.value=second==10?0:100;s.lux.value=second==10?0:100;s.wind.value=second==10?1:8;
      assert(!a.observe(s,weather::Result::Accepted,0,0,false));
      assert(!a.observe(s,weather::Result::Duplicate,0,0,false));
    }
    if(second==5||second==290){
      weather::Sample s;s.time_ms=now;s.type=38;s.rain_tips.valid=true;
      assert(!a.observe(s,weather::Result::Accepted,second==5?0:2,second==5?start-5000:start+5000,true));
    }
  }
  auto completed=a.tick(start+kIntervalMs,true);assert(completed);
  auto r=*completed;assert(sizeof(r)==64&&valid(r,679,'A'));
  assert(r.temperature_count==2&&r.temperature_mean==15&&r.temperature_min==10&&r.temperature_max==20);
  assert(r.humidity_tenths==500&&r.lux_mean==50&&r.lux_peak==100&&r.wind_mean==4.5&&r.wind_peak==8);
  assert(r.accepted==4&&r.repeats==2&&r.observed_seconds==300&&r.current_seconds==240&&r.rain_tips==2);
  assert(!(r.flags&kPartial)&&!(r.flags&kRainPartial));
  r.lux_peak^=1;assert(!valid(r,679,'A'));r=*completed;assert(!valid(r,680,'A')&&!valid(r,679,'B'));
  assert(inWindow(100,100)&&inWindow(100,100+kRecords-1)&&!inWindow(100,100+kRecords)&&!inWindow(101,100));
  // Missing fields remain distinguishable from real zero; skipped time is a gap.
  auto gap=a.tick(start+3*kIntervalMs,false);assert(gap&&gap->observed_seconds==1&&(gap->flags&kPartial));
  assert(gap->temperature_count==0&&gap->rain_samples==0&&(gap->flags&kRainPartial));
  auto before=a.snapshot();assert(!a.tick(start+kIntervalMs,false));assert(a.snapshot().interval==before.interval);
  // A rain interval crossing a five-minute boundary has uncertain allocation.
  Accumulator rain;rain.tick(start,false);weather::Sample s;s.time_ms=start+5000;s.rain_tips.valid=true;
  rain.observe(s,weather::Result::Accepted,0,start-5000,true);
  s.time_ms=start+kIntervalMs+1000;auto crossed=rain.observe(s,weather::Result::Accepted,1,start+kIntervalMs-1000,true);
  assert(crossed&&(crossed->flags&kRainPartial));assert(rain.snapshot().flags&kRainPartial);
  // Reopening a saved partial retains earlier means/counts and marks the gap.
  Accumulator restored;assert(restored.resume(*completed));s=weather::Sample{};s.time_ms=start+299000;s.temperature.valid=true;s.temperature.value=30;
  restored.observe(s,weather::Result::Accepted,0,0,false);r=restored.snapshot();
  assert(r.temperature_count==3&&std::fabs(r.temperature_mean-20)<.001&&(r.flags&kPartial)&&(r.flags&kRainPartial));
  // Rejected frames don't contribute measurements; another station is ignored.
  Accumulator rejected;s.time_ms=start;s.station=680;rejected.observe(s,weather::Result::Accepted,0,0,false);assert(!rejected.active());
  s.station=679;rejected.observe(s,weather::Result::Flagged,0,0,false);assert(rejected.snapshot().rejected==1&&rejected.snapshot().temperature_count==0);
  for(unsigned n=0;n<70000;++n)rejected.observe(s,weather::Result::Duplicate,0,0,false);
  assert(rejected.snapshot().repeats==UINT16_MAX&&(rejected.snapshot().flags&kSaturated));
  // A normal 1.01-second main-loop cadence is continuous coverage, not 297
  // seconds of uptime. Actual multi-second pauses must still leave a gap.
  Accumulator jitter;
  for(uint64_t elapsed=0;elapsed<kIntervalMs;elapsed+=1010)jitter.tick(start+elapsed,true);
  auto continuous=jitter.tick(start+kIntervalMs+1000,true);
  assert(continuous&&continuous->observed_seconds==300&&continuous->current_seconds==300&&!(continuous->flags&kPartial));
  Accumulator paused;paused.tick(start,true);paused.tick(start+10000,true);
  auto incomplete=paused.tick(start+kIntervalMs,true);
  assert(incomplete&&incomplete->observed_seconds==2&&(incomplete->flags&kPartial));
  std::cout<<"Archive checks passed: 64-byte records, means/peaks, zero/missing, coverage/gaps, seven-day expiry, CRC, binding, rain boundaries, restart merge and saturation.\n";
}
