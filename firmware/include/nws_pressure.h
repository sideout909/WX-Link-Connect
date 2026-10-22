#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
namespace nws_pressure {
constexpr int64_t kMaxAge=90*60;
inline bool fresh(int64_t observed,int64_t now){return observed>0&&now>=observed&&now-observed<kMaxAge;}
inline bool pascals(double value,const char* unit,bool sea_level,float& out){
  if(!std::isfinite(value)||!unit)return false;
  if(!std::strcmp(unit,"wmoUnit:hPa"))value*=100;
  else if(!std::strcmp(unit,"wmoUnit:inHg"))value*=3386.389;
  else if(std::strcmp(unit,"wmoUnit:Pa"))return false;
  if(value<(sea_level?80000:20000)||value>110000)return false;
  out=value;return true;
}
}
