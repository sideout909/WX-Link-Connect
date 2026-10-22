#include "nws_pressure.h"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
  float pa=0;
  assert(nws_pressure::pascals(101325,"wmoUnit:Pa",true,pa)&&std::abs(pa/3386.389-29.9213)<.001);
  assert(nws_pressure::pascals(1013.25,"wmoUnit:hPa",true,pa)&&pa==101325);
  assert(nws_pressure::pascals(29.92,"wmoUnit:inHg",false,pa));
  assert(nws_pressure::pascals(65000,"wmoUnit:Pa",false,pa));
  assert(!nws_pressure::pascals(65000,"wmoUnit:Pa",true,pa));
  assert(!nws_pressure::pascals(0,"wmoUnit:Pa",false,pa));
  assert(!nws_pressure::pascals(1013,"unknown",true,pa));
  assert(!nws_pressure::pascals(std::numeric_limits<double>::quiet_NaN(),"wmoUnit:Pa",true,pa));
  assert(nws_pressure::fresh(10000,15399));assert(!nws_pressure::fresh(10000,15400));
  assert(!nws_pressure::fresh(10000,9999));assert(!nws_pressure::fresh(0,10000));
  std::cout<<"NWS pressure checks passed: units, sea-level/reported bounds, missing/NaN, future and expiration.\n";
}
