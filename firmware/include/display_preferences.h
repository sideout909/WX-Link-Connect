#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>

namespace display_preferences {
// Standard UV Index bands; classify the displayed rounded index consistently.
inline const char* uvLevel(float uv){
  if(!std::isfinite(uv)||uv<0)return "Unavailable";
  const int index=int(std::lround(uv));
  return index<3?"Low":index<6?"Moderate":index<8?"High":index<11?"Very High":"Extreme";
}
inline bool validDate(const char* format){return !std::strcmp(format,"md")||!std::strcmp(format,"dm")||!std::strcmp(format,"iso")||!std::strcmp(format,"text");}
inline bool validSleep(const char* mode){return !std::strcmp(mode,"never")||!std::strcmp(mode,"dim")||!std::strcmp(mode,"off");}
inline bool validTimeout(unsigned seconds){return seconds==30||seconds==60||seconds==120||seconds==300||seconds==600||seconds==900||seconds==1800||seconds==3600;}
inline void clockText(char* out,size_t size,const tm& local,bool clock_24h){
  std::strftime(out,size,clock_24h?"%H:%M":"%I:%M %p",&local);
  if(!clock_24h&&out[0]=='0')std::memmove(out,out+1,std::strlen(out));
}
inline void headerText(char* out,size_t size,const tm& local,const char* date_format,bool clock_24h){
  char date[16],clock[16];
  const char* format=!std::strcmp(date_format,"dm")?"%d/%m":!std::strcmp(date_format,"iso")?"%Y-%m-%d":!std::strcmp(date_format,"text")?"%b %d":"%m/%d";
  std::strftime(date,sizeof(date),format,&local);clockText(clock,sizeof(clock),local,clock_24h);
  std::snprintf(out,size,"%s %s",date,clock);
}
struct Idle {
  uint64_t last_interaction=0;
  void interact(uint64_t now){last_interaction=now;}
  uint8_t brightness(uint64_t now,const char* mode,unsigned seconds,uint8_t normal=160)const{
    if(!std::strcmp(mode,"never")||now<last_interaction||now-last_interaction<uint64_t(seconds)*1000)return normal;
    return !std::strcmp(mode,"dim")?uint8_t(normal/10?normal/10:1):0;
  }
  bool returnHome(uint64_t now,bool eligible,bool touched)const{return eligible&&!touched&&now>=last_interaction&&now-last_interaction>=60000;}
};
inline bool setupComplete(bool home_wifi_saved,bool station_confirmed){return home_wifi_saved&&station_confirmed;}
struct StatusLayout {
  int rf,wifi,aprs;
  int wifi_left,wifi_right;
};
inline StatusLayout statusLayout(int rf_width,int wifi_width,int aprs_width){
  StatusLayout layout{};layout.aprs=310-aprs_width;
  layout.wifi=layout.aprs-6-wifi_width;layout.rf=layout.wifi-6-rf_width;
  layout.wifi_left=layout.wifi-8;layout.wifi_right=layout.aprs-2;
  return layout;
}
}
