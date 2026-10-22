#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include "atlas_packet.h"
#include "station_discovery.h"

namespace station_discovery {
// Call only for trusted, native acurite_txr callbacks after upstream integrity
// validation. Atlas intentionally has no mic key. USB replay must never enter.
inline bool decodeNative(JsonVariantConst d,Packet& packet){
  const char* model=d["model"]|"";
  unsigned max_id=0;
  if(!std::strcmp(model,"Acurite-Atlas")){packet=Packet{};packet.model=Model::Atlas;max_id=1023;}
  else if(!std::strcmp(model,"Acurite-5n1")){packet=Packet{};packet.model=Model::Iris;max_id=4095;}
  else if(!std::strcmp(model,"Acurite-3n1")){packet=Packet{};packet.model=Model::Notos;max_id=16383;}
  else return false;
  const char* channel=d["channel"]|"";
  if(!d["id"].is<unsigned>()||d["id"].as<unsigned>()>max_id||std::strlen(channel)!=1||!std::strchr("ABC",channel[0])||
     !d["message_type"].is<unsigned>()||d["exception"].as<bool>())return false;
  const unsigned type=d["message_type"].as<unsigned>();
  if(packet.model==Model::Atlas){
    weather::Sample sample;if(!atlas::decode(d,1,sample))return false;
    if(sample.temperature.valid)packet.fields|=Temperature;
    if(sample.humidity.valid)packet.fields|=Humidity;
    if(sample.wind.valid)packet.fields|=Wind;
    if(sample.direction.valid)packet.fields|=Direction;
    if(sample.rain_tips.valid)packet.fields|=Rain;
    if(sample.lux.valid)packet.fields|=Light;
    if(sample.uv.valid)packet.fields|=UV;
    packet.temperature_f=sample.temperature.value;packet.humidity=sample.humidity.value;packet.wind_mph=sample.wind.value;
  }else{
    if(std::strcmp(d["mic"]|"","CHECKSUM"))return false;
    if(packet.model==Model::Iris&&(type!=49&&type!=56))return false;
    if(packet.model==Model::Notos&&type!=32)return false;
    if(d["temperature_F"].is<float>()){packet.temperature_f=d["temperature_F"].as<float>();packet.fields|=Temperature;}
    if(d["humidity"].is<unsigned>()&&d["humidity"].as<unsigned>()<=100){packet.humidity=d["humidity"].as<unsigned>();packet.fields|=Humidity;}
    if(d["wind_avg_mi_h"].is<float>()){packet.wind_mph=d["wind_avg_mi_h"].as<float>();packet.fields|=Wind;}
    else if(d["wind_avg_km_h"].is<float>()){packet.wind_mph=d["wind_avg_km_h"].as<float>()/1.609344f;packet.fields|=Wind;}
    if(packet.model==Model::Iris&&type==49){
      if(!d["rain_in"].is<float>()||!d["wind_dir_deg"].is<float>()||!std::isfinite(d["rain_in"].as<float>())||d["rain_in"].as<float>()<0||d["rain_in"].as<float>()>163.83f||
        !std::isfinite(d["wind_dir_deg"].as<float>())||d["wind_dir_deg"].as<float>()<0||d["wind_dir_deg"].as<float>()>=360)return false;
      packet.fields|=Direction|Rain;
    }else if((packet.fields&(Temperature|Humidity|Wind))!=(Temperature|Humidity|Wind))return false;
  }
  if(!(packet.fields&Wind)||!std::isfinite(packet.wind_mph)||packet.wind_mph<0||packet.wind_mph>200)return false;
  if((packet.fields&Temperature)&&(!std::isfinite(packet.temperature_f)||packet.temperature_f< -40||packet.temperature_f>158))return false;
  if((packet.fields&Humidity)&&packet.humidity>100)return false;
  packet.id=d["id"].as<unsigned>();packet.channel=channel[0];return true;
}
}
