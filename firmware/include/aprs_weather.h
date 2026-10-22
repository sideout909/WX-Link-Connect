#pragma once
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
namespace aprs {
struct Weather {
  time_t epoch=0;double latitude=0,longitude=0;
  bool temperature_valid=false,humidity_valid=false,gust_valid=false,direction_valid=false;
  float temperature_f=0,gust_mph=0,direction_deg=0;unsigned humidity=0;
  bool rain_hour_valid=false,rain_24h_valid=false,rain_today_valid=false;
  float rain_hour=0,rain_24h=0,rain_today=0;
};
inline bool identity(const std::string& call){
  if(call.size()<3 || call.size()>9)return false;size_t dash=call.find('-');
  if(dash!=std::string::npos && (dash<3||call.size()-dash>3||dash==call.size()-1))return false;
  for(size_t i=0;i<call.size();++i)if(!((call[i]>='A'&&call[i]<='Z')||(call[i]>='0'&&call[i]<='9')||(call[i]=='-'&&i==dash)))return false;
  return true;
}
inline std::string position(double degrees,bool latitude){
  const long hundredths=std::lround(std::fabs(degrees)*6000);const long whole=hundredths/6000,minutes=hundredths%6000;
  char b[24];std::snprintf(b,sizeof(b),latitude?"%02ld%02ld.%02ld%c":"%03ld%02ld.%02ld%c",whole,minutes/100,minutes%100,latitude?(degrees<0?'S':'N'):(degrees<0?'W':'E'));return b;
}
inline std::string packet(const std::string& call,const Weather& w){
  if(!identity(call)||w.epoch<1700000000||!std::isfinite(w.latitude)||!std::isfinite(w.longitude)||std::fabs(w.latitude)>90||std::fabs(w.longitude)>180)return {};
  struct tm utc{};gmtime_r(&w.epoch,&utc);char b[64];std::strftime(b,sizeof(b),"%d%H%Mz",&utc);
  std::string out=call+">APRS,TCPIP*:@"+b+position(w.latitude,true)+"/"+position(w.longitude,false)+"_";
  if(w.direction_valid && std::isfinite(w.direction_deg)&&w.direction_deg>=0&&w.direction_deg<=360){int d=int(std::lround(w.direction_deg))%360;std::snprintf(b,sizeof(b),"%03d",d?d:360);out+=b;}else out+="...";
  // Atlas packets do not establish a one-minute sustained wind average.
  out+="/...";
  if(w.gust_valid&&std::isfinite(w.gust_mph)&&w.gust_mph>=0&&w.gust_mph<=999){std::snprintf(b,sizeof(b),"g%03d",int(std::lround(w.gust_mph)));out+=b;}
  if(w.temperature_valid&&std::isfinite(w.temperature_f)&&w.temperature_f>=-99&&w.temperature_f<=999){std::snprintf(b,sizeof(b),"t%03d",int(std::lround(w.temperature_f)));out+=b;}
  if(w.humidity_valid&&w.humidity>0&&w.humidity<=100){std::snprintf(b,sizeof(b),"h%02u",w.humidity==100?0:w.humidity);out+=b;}
  auto rain=[&](char code,bool valid,float inches){if(valid&&std::isfinite(inches)&&inches>=0&&inches<9.995f){std::snprintf(b,sizeof(b),"%c%03d",code,int(std::lround(inches*100)));out+=b;}};
  rain('r',w.rain_hour_valid,w.rain_hour);rain('p',w.rain_24h_valid,w.rain_24h);rain('P',w.rain_today_valid,w.rain_today);
  out+=" CYDWeatherLink\r\n";return out.size()<=512?out:std::string{};
}
}
