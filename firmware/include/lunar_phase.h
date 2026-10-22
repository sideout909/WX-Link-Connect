#pragma once
#include <cmath>
#include <cstdint>
#include <ctime>

namespace lunar_phase {
// Mean synodic month, NASA: https://eclipse.gsfc.nasa.gov/phase/phasecat.html
// A small-icon estimate anchored to the provider's date/illumination, not an ephemeris.
constexpr double kCycleSeconds=29.530588*86400;
struct Phase {float illumination=-1;bool waning=false;};
inline Phase advance(float illumination,bool waning,double elapsed_seconds){
  if(!std::isfinite(illumination)||illumination<0||illumination>100||
     !std::isfinite(elapsed_seconds)||elapsed_seconds < -86400||elapsed_seconds>8*86400)return {};
  constexpr double pi=3.14159265358979323846;
  double angle=std::acos(1-2*double(illumination)/100);
  if(waning)angle=2*pi-angle;
  angle=std::fmod(angle+2*pi*elapsed_seconds/kCycleSeconds,2*pi);
  if(angle<0)angle+=2*pi;
  return {float(50*(1-std::cos(angle))),angle>pi};
}
inline time_t referenceNoon(int32_t day){
  if(day<20200101||day>21001231)return 0;
  tm local{};local.tm_year=day/10000-1900;local.tm_mon=(day/100)%100-1;
  local.tm_mday=day%100;local.tm_hour=12;local.tm_isdst=-1;
  return std::mktime(&local);
}
}
