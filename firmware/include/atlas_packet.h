#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include "weather_store.h"
namespace atlas {
template <typename T> inline void load(JsonVariantConst v,weather::Field<T>& field){if(v.is<T>()){field.valid=true;field.value=v.as<T>();}}
inline bool decode(JsonVariantConst d,uint64_t now,weather::Sample& s){
  const char* model=d["model"]|"";if(std::strcmp(model,"Acurite-Atlas"))return false;
  const char* bound_channel=d["channel"]|"";
  if(!d["id"].is<uint16_t>() || d["id"].as<unsigned>()>1023 || std::strlen(bound_channel)!=1 || !d["message_type"].is<uint8_t>())return false;
  unsigned type=d["message_type"].as<unsigned>();if(type>=5&&type<=7)type+=32;if(type<37||type>39)return false;
  s=weather::Sample{};s.time_ms=now;s.station=d["id"]|uint16_t(0);const char* channel=d["channel"]|"";s.channel=channel[0];
  s.type=uint8_t(type);s.exception=d["exception"].as<bool>();
  load(d["temperature_F"],s.temperature);
  if(!s.temperature.valid&&d["temperature_C"].is<float>()){s.temperature.valid=true;s.temperature.value=d["temperature_C"].as<float>()*1.8f+32;}
  load(d["humidity"],s.humidity);load(d["wind_avg_mi_h"],s.wind);
  if(!s.wind.valid&&d["wind_avg_km_h"].is<float>()){s.wind.valid=true;s.wind.value=d["wind_avg_km_h"].as<float>()/1.609344f;}
  if(!s.wind.valid&&d["wind_avg_m_s"].is<float>()){s.wind.valid=true;s.wind.value=d["wind_avg_m_s"].as<float>()*2.2369363f;}
  load(d["wind_dir_deg"],s.direction);load(d["lux"],s.lux);load(d["uv"],s.uv);
  if(!s.uv.valid)load(d["uvi"],s.uv);
  load(d["strike_count"],s.strikes);
  if(d["rain_in"].is<float>()){const float rain=d["rain_in"].as<float>();if(!std::isfinite(rain)||rain<0||rain>5.11f)return false;s.rain_tips.valid=true;s.rain_tips.value=std::lround(rain*100);}
  else if(d["rain_mm"].is<float>()){const float rain=d["rain_mm"].as<float>();if(!std::isfinite(rain)||rain<0||rain>129.8f)return false;s.rain_tips.valid=true;s.rain_tips.value=std::lround(rain/0.254f);}
  return true;
}
}
