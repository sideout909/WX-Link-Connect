#include "atlas_packet.h"
#include "sensor_health.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv){
  StaticJsonDocument<2048> d;weather::Sample s;weather::Store store;
  assert(!deserializeJson(d,R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":37,"temperature_C":20.5,"humidity":0,"wind_avg_km_h":16.09344})"));
  assert(atlas::decode(d.as<JsonVariantConst>(),1000000,s));assert(std::fabs(s.temperature.value-68.9)<.001);assert(std::fabs(s.wind.value-10)<.001);assert(s.humidity.valid&&s.humidity.value==0);assert(!s.lux.valid);assert(store.ingest(s)==weather::Result::Accepted);
  assert(!deserializeJson(d,R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":38,"rain_mm":6.35,"wind_avg_m_s":0,"wind_dir_deg":360,"exception":1})"));
  assert(atlas::decode(d.as<JsonVariantConst>(),1030000,s));assert(s.exception&&s.rain_tips.value==25&&s.wind.valid&&s.wind.value==0);assert(store.ingest(s)==weather::Result::Flagged);
  assert(!deserializeJson(d,R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":39,"lux":0,"uvi":0,"wind_avg_mi_h":4,"temperature_F":42,"exception":0})"));
  assert(atlas::decode(d.as<JsonVariantConst>(),1060000,s));assert(!s.exception&&s.lux.valid&&s.lux.value==0&&s.uv.valid&&s.uv.value==0&&s.temperature.value==42);assert(store.ingest(s)==weather::Result::Accepted);
  d["rain_in"]=-.01;assert(!atlas::decode(d.as<JsonVariantConst>(),1090000,s));d.remove("rain_in");d["model"]="Acurite-5n1";assert(!atlas::decode(d.as<JsonVariantConst>(),1090000,s));
  // The pinned embedded decoder emits "uv"; desktop captures can use "uvi".
  assert(!deserializeJson(d,R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":39,"lux":0,"uv":0,"wind_avg_mi_h":0})"));
  assert(atlas::decode(d.as<JsonVariantConst>(),1100000,s));assert(s.uv.valid&&s.uv.value==0);
  d["uv"]=7;assert(atlas::decode(d.as<JsonVariantConst>(),1110000,s));assert(s.uv.valid&&s.uv.value==7);
  d["uvi"]=3;assert(atlas::decode(d.as<JsonVariantConst>(),1120000,s));assert(s.uv.value==7);
  d.remove("uv");assert(atlas::decode(d.as<JsonVariantConst>(),1130000,s));assert(s.uv.valid&&s.uv.value==3);
  d.remove("uvi");assert(atlas::decode(d.as<JsonVariantConst>(),1140000,s));assert(!s.uv.valid);
  weather::Store live; sensor_health::Monitor monitor;monitor.reset(1200000);
  const char* cycle[]={
    R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":37,"temperature_F":45.4,"humidity":94,"wind_avg_mi_h":0})",
    R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":38,"rain_in":0.25,"wind_dir_deg":180,"wind_avg_mi_h":0})",
    R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":39,"lux":0,"uv":0,"wind_avg_mi_h":0})"};
  uint64_t stamp=1200000;for(auto json:cycle){assert(!deserializeJson(d,json));assert(atlas::decode(d.as<JsonVariantConst>(),stamp++,s));monitor.observe(s,live.ingest(s));}
  assert(monitor.state(true,true,stamp,live)==sensor_health::State::Current);
  unsigned accepted=0,duplicates=0,flagged=0,other=0,parsed=0;
  // Exercise the same live parser against a complete original SDR capture.
  if(argc>1){std::ifstream file(argv[1]);assert(file.good());std::string line;weather::Store reference;uint64_t now=1700000000000ULL;
    while(std::getline(file,line)){d.clear();assert(!deserializeJson(d,line));++parsed;if(!atlas::decode(d.as<JsonVariantConst>(),now++,s)){++other;continue;}auto result=reference.ingest(s);if(result==weather::Result::Accepted)++accepted;else if(result==weather::Result::Duplicate)++duplicates;else if(result==weather::Result::Flagged)++flagged;else assert(false);}
  }
  std::cout<<"Atlas live adapter checks passed: SI/native units, zeros, sparse fields, integer exception flag, bounds and model binding. Capture packets="<<parsed<<", accepted="<<accepted<<", repeats="<<duplicates<<", flagged="<<flagged<<", other="<<other<<".\n";
}
