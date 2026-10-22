#pragma once
#include <cstdint>
namespace weather {
struct DailyRain {
  struct Total { uint32_t tips=0;bool valid=false,complete=false; };
  int32_t day=0;
  uint32_t tips=0;
  bool valid=false,complete=false;
  uint64_t last_time=0;
  // Present the new date immediately at midnight, even before another rain
  // packet arrives. Keep the previous observation for cross-midnight gap and
  // tip-allocation checks; changing the display must not rewrite the baseline.
  Total today(int32_t local_day,uint64_t now)const {
    if(!valid || !local_day || local_day<day || now<last_time)return {};
    if(local_day!=day)return {0,true,false};
    return {tips,true,complete && now-last_time<120000};
  }
  // Local date is supplied by the timezone-aware caller. Never infer today's
  // rain from the station's lifetime/modulo counter.
  void observe(int32_t local_day,uint64_t now,uint32_t delta,bool continuous) {
    if(!local_day)return;
    if(!valid){day=local_day;valid=true;complete=false;tips=0;}
    else if(local_day!=day){
      const bool crossed=continuous && now>=last_time && now-last_time<=120000;
      day=local_day;tips=0;complete=crossed && delta==0;
      // Rain spanning midnight cannot be assigned to either date exactly.
      if(delta)continuous=false;
    }
    tips+=delta;
    if(!continuous)complete=false;
    last_time=now;
  }
};
}
