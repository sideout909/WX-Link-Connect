#pragma once
#include "weather_store.h"
#include <array>
namespace sensor_health {
constexpr uint64_t kDataStaleMs = 600000;
constexpr uint64_t kDisconnectedMs = 300000;
enum class State { Off,NoRadio,Waiting,Collecting,Current,DataStale,NoSignal };
inline const char* label(State state){
  switch(state){case State::Off:return "Receiver off";case State::NoRadio:return "No receiver";case State::Waiting:return "Waiting";case State::Collecting:return "Receiving";case State::Current:return "Current";case State::DataStale:return "Data stale";case State::NoSignal:return "Disconnected";}
  return "Unknown";
}
struct Monitor {
  uint64_t started=0,last_rx=0,last_change=0;
  std::array<uint64_t,3> types{};
  uint32_t received=0;
  weather::Sample values{};
  template<typename T> static bool update(weather::Field<T>& previous,const weather::Field<T>& next){
    if(!next.valid)return false;
    const bool changed=!previous.valid||previous.value!=next.value;
    previous=next;return changed;
  }
  void reset(uint64_t now){*this=Monitor{};started=now;}
  void observe(const weather::Sample& packet,weather::Result result){
    if(!packet.time_ms||packet.type<37||packet.type>39)return;
    if(result!=weather::Result::Accepted&&result!=weather::Result::Duplicate&&result!=weather::Result::Flagged)return;
    if(last_rx&&packet.time_ms<last_rx)return;
    if(!received)started=packet.time_ms;
    last_rx=packet.time_ms;++received;
    if(result==weather::Result::Accepted){
      types[packet.type-37]=packet.time_ms;
      bool changed=false;
      changed|=update(values.temperature,packet.temperature);
      changed|=update(values.humidity,packet.humidity);
      changed|=update(values.wind,packet.wind);
      changed|=update(values.direction,packet.direction);
      changed|=update(values.lux,packet.lux);
      changed|=update(values.uv,packet.uv);
      changed|=update(values.rain_tips,packet.rain_tips);
      changed|=update(values.strikes,packet.strikes);
      if(changed)last_change=packet.time_ms;
    }
  }
  bool complete(uint64_t now,const weather::Store& store,uint64_t max_age=weather::kStaleMs)const {
    for(auto time:types)if(!time||now<time||now-time>=max_age)return false;
    const auto rain=store.rainState();
    return store.temperature.fresh(now,max_age)&&store.humidity.fresh(now,max_age)&&
      store.wind.fresh(now,max_age)&&store.direction.fresh(now,max_age)&&
      store.lux.fresh(now,max_age)&&store.uv.fresh(now,max_age)&&
      rain.baseline&&now>=rain.time&&now-rain.time<max_age;
  }
  State state(bool enabled,bool ready,uint64_t now,const weather::Store& store)const {
    if(!enabled)return State::Off;
    if(!ready)return State::NoRadio;
    if(!received)return now>=started&&now-started>=kDisconnectedMs?State::NoSignal:State::Waiting;
    if(now<last_rx||now-last_rx>=kDisconnectedMs)return State::NoSignal;
    // A ten-minute unchanged interval is a warning, not proof of a sensor freeze.
    if(last_change&&now>=last_change&&now-last_change>=kDataStaleMs)return State::DataStale;
    if(complete(now,store,kDataStaleMs))return State::Current;
    return now>=started&&now-started>=kDataStaleMs?State::DataStale:State::Collecting;
  }
};
}
