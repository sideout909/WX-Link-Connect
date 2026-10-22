#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>

namespace station_discovery {
constexpr uint32_t kDurationMs=300000,kFreshMs=120000,kBurstMs=6000;
constexpr size_t kCapacity=16;
enum class Model:uint8_t { Atlas,Iris,Notos };
inline const char* name(Model model){return model==Model::Atlas?"AcuRite Atlas":model==Model::Iris?"AcuRite Iris / 5-in-1":"AcuRite Notos / 3-in-1";}
enum Field:uint8_t { Temperature=1,Humidity=2,Wind=4,Direction=8,Rain=16,Light=32,UV=64 };
struct Packet {
  Model model=Model::Atlas;
  uint16_t id=0;
  char channel='A';
  uint8_t fields=0;
  float temperature_f=0,wind_mph=0;
  uint8_t humidity=0;
};
struct Candidate {
  Packet packet{};
  uint32_t token=0,first_ms=0,last_ms=0,counted_ms=0;
  std::array<uint32_t,7> field_ms{};
  uint16_t sightings=0;
  bool occupied=false;
  bool fresh(uint32_t now)const{return occupied&&uint32_t(now-last_ms)<kFreshMs;}
  uint8_t freshFields(uint32_t now)const{
    uint8_t mask=0;for(unsigned i=0;i<7;++i)if((packet.fields&(1<<i))&&uint32_t(now-field_ms[i])<kFreshMs)mask|=1<<i;return mask;
  }
  bool suggested(uint32_t now)const{return fresh(now)&&sightings>=3;}
  bool selectable(uint32_t now)const{
    // Only a validated operational adapter can be bound. Other native AcuRite
    // families are useful detections, not yet supported weather-store profiles.
    return packet.model==Model::Atlas&&suggested(now)&&freshFields(now)==127;
  }
};
class Scan {
 public:
  void start(uint32_t now,uint32_t session){*this=Scan{};started_=now;session_=session?session:1;active_=true;}
  void cancel(){active_=false;cancelled_=true;}
  bool active(uint32_t now)const{return active_&&uint32_t(now-started_)<kDurationMs;}
  bool expired(uint32_t now)const{return session_&&!cancelled_&&!active(now)&&uint32_t(now-started_)>=kDurationMs;}
  uint32_t elapsed(uint32_t now)const{return session_?std::min(uint32_t(now-started_),kDurationMs):0;}
  uint32_t session()const{return session_;}
  bool observe(const Packet& packet,uint32_t now){
    if(!active(now)||!packet.fields)return false;
    Candidate* slot=nullptr;
    for(auto& candidate:candidates_)if(candidate.occupied&&candidate.packet.model==packet.model&&candidate.packet.id==packet.id&&candidate.packet.channel==packet.channel){slot=&candidate;break;}
    if(!slot)for(auto& candidate:candidates_)if(!candidate.occupied){slot=&candidate;break;}
    // A bounded scan never silently reuses a visible candidate token.
    if(!slot){++overflow_;return false;}
    if(!slot->occupied){*slot=Candidate{};slot->occupied=true;slot->packet=packet;slot->token=++next_token_;slot->first_ms=now;}
    if(!slot->sightings||uint32_t(now-slot->counted_ms)>=kBurstMs){if(slot->sightings<65535)++slot->sightings;slot->counted_ms=now;}
    slot->last_ms=now;
    for(unsigned i=0;i<7;++i)if(packet.fields&(1<<i))slot->field_ms[i]=now;
    slot->packet.fields|=packet.fields;
    if(packet.fields&Temperature)slot->packet.temperature_f=packet.temperature_f;
    if(packet.fields&Humidity)slot->packet.humidity=packet.humidity;
    if(packet.fields&Wind)slot->packet.wind_mph=packet.wind_mph;
    return true;
  }
  const Candidate* choose(uint32_t session,uint32_t token,uint32_t now)const{
    if(!session||session!=session_||cancelled_)return nullptr;
    for(const auto& candidate:candidates_)if(candidate.token==token&&candidate.selectable(now))return &candidate;
    return nullptr;
  }
  const std::array<Candidate,kCapacity>& candidates()const{return candidates_;}
  uint32_t overflow()const{return overflow_;}
 private:
  std::array<Candidate,kCapacity> candidates_{};
  uint32_t started_=0,session_=0,next_token_=0,overflow_=0;
  bool active_=false,cancelled_=false;
};
}
