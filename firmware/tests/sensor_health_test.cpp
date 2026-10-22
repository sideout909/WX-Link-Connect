#include <string>
#include "sensor_health.h"
#include <cassert>
#include <iostream>
int main(){
  assert(std::string(sensor_health::label(sensor_health::State::Collecting))=="Receiving");
  using S=sensor_health::State;
  constexpr uint64_t start=1000000;
  weather::Store store;sensor_health::Monitor monitor;monitor.reset(start);
  auto packet=[](uint64_t now,uint8_t type){
    weather::Sample s;s.time_ms=now;s.type=type;s.wind.set(8.2f,now);
    if(type==37){s.temperature.set(56.3f,now);s.humidity.set(58,now);}
    if(type==38){s.rain_tips.set(100,now);s.direction.set(225,now);}
    if(type==39){s.lux.set(24000,now);s.uv.set(2,now);}return s;
  };
  auto feed=[&](weather::Sample p){monitor.observe(p,store.ingest(p));};
  assert(monitor.state(false,false,start,store)==S::Off);
  assert(monitor.state(true,false,start,store)==S::NoRadio);
  assert(monitor.state(true,true,start+299999,store)==S::Waiting);
  assert(monitor.state(true,true,start+300000,store)==S::NoSignal);
  assert(std::string(sensor_health::label(S::NoSignal))=="Disconnected");
  for(unsigned i=0;i<3;++i)feed(packet(start+i*10000,37+i));
  assert(monitor.state(true,true,start+20000,store)==S::Current);
  feed(packet(start+21000,39));
  assert(store.duplicates==1&&monitor.last_rx==start+21000);
  assert(monitor.last_change==start+20000);
  // Display tolerates gaps longer than two minutes; upload completeness does not.
  assert(monitor.state(true,true,start+200000,store)==S::Current);
  assert(!monitor.complete(start+200000,store));
  assert(store.temperature.fresh(start+200000,sensor_health::kDataStaleMs));
  assert(!store.temperature.fresh(start+200000));
  // No packets: five-minute boundary, before the unchanged warning.
  assert(monitor.state(true,true,start+320999,store)==S::Current);
  assert(monitor.state(true,true,start+321000,store)==S::NoSignal);
  // All types still arriving with identical values: warn at exactly ten minutes.
  for(unsigned t=30000;t<=610000;t+=10000)feed(packet(start+t,37+(t/10000)%3));
  assert(monitor.complete(start+619999,store));
  assert(monitor.state(true,true,start+619999,store)==S::Current);
  assert(monitor.state(true,true,start+620000,store)==S::DataStale);
  auto changed=packet(start+620000,37);changed.temperature.value+=0.1f;feed(changed);
  assert(monitor.last_change==start+620000);
  assert(monitor.state(true,true,start+620000,store)==S::Current);
  // Foreign/invalid packets cannot keep this station connected or reset changes.
  auto wrong=packet(start+700000,38);wrong.station=999;feed(wrong);
  auto invalid=packet(start+710000,37);invalid.temperature.value=999;feed(invalid);
  assert(monitor.last_rx==start+620000&&monitor.last_change==start+620000);
  assert(monitor.state(true,true,start+920000,store)==S::NoSignal);
  // Flagged bound-station packets prove reception but cannot refresh weather.
  auto bad=packet(start+930000,37);bad.exception=true;feed(bad);
  assert(monitor.last_rx==start+930000&&monitor.last_change==start+620000);
  assert(!monitor.complete(start+930000,store));
  // A missing type goes stale at ten minutes even if another value changes.
  monitor.reset(start);weather::Store partial;
  for(unsigned t=0;t<600000;t+=10000){auto p=packet(start+t,37);p.wind.value=float(t/10000);monitor.observe(p,partial.ingest(p));}
  assert(monitor.state(true,true,start+599999,partial)==S::Collecting);
  auto changing=packet(start+600000,37);changing.wind.value=60;
  monitor.observe(changing,partial.ingest(changing));
  assert(monitor.state(true,true,start+600000,partial)==S::DataStale);
  for(unsigned i=0;i<3;++i){auto p=packet(start+610000+i*10000,37+i);monitor.observe(p,partial.ingest(p));}
  assert(monitor.state(true,true,start+630000,partial)==S::Current);
  monitor.reset(start);assert(monitor.received==0&&monitor.last_rx==0&&monitor.last_change==0);
  std::cout<<"Sensor health checks passed: ten-minute unchanged/missing data, five-minute disconnect, recovery, duplicates, invalid and flagged packets, separate upload freshness.\n";
}
