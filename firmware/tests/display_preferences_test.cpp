#include "display_preferences.h"
#include "display_themes.h"
#include "moon_icon.h"
#include "lunar_phase.h"
#include <cmath>
#include "forecast_icon.h"
#include <cassert>
#include <iostream>
#include <limits>

int main(){
  using namespace display_preferences;
  Idle idle;idle.interact(1000);
  assert(idle.brightness(30999,"dim",30)==160);assert(idle.brightness(31000,"dim",30)==16);
  assert(idle.brightness(31000,"off",30)==0);assert(idle.brightness(999999,"never",30)==160);
  assert(!idle.returnHome(60999,true,false));assert(idle.returnHome(61000,true,false));
  assert(!idle.returnHome(61000,false,false));assert(!idle.returnHome(61000,true,true));
  idle.interact(60999);assert(!idle.returnHome(61000,true,false));assert(idle.brightness(61000,"off",30)==160);
  assert(idle.brightness(5,"off",30)==160); // Clock moving backward cannot darken it.
  idle.interact(uint64_t(UINT32_MAX)+1000);assert(idle.returnHome(uint64_t(UINT32_MAX)+61000,true,false));
  tm local{};local.tm_year=126;local.tm_mon=9;local.tm_mday=21;local.tm_hour=0;local.tm_min=5;
  char clock[40];headerText(clock,sizeof(clock),local,"md",false);assert(!std::strcmp(clock,"10/21 12:05 AM"));
  headerText(clock,sizeof(clock),local,"dm",true);assert(!std::strcmp(clock,"21/10 00:05"));
  local.tm_hour=13;headerText(clock,sizeof(clock),local,"iso",false);assert(!std::strcmp(clock,"2026-10-21 1:05 PM"));
  headerText(clock,sizeof(clock),local,"text",true);assert(!std::strcmp(clock,"Oct 21 13:05"));
  assert(validDate("md")&&!validDate("x"));assert(validSleep("dim")&&!validSleep("deep_sleep"));
  assert(validTimeout(30)&&validTimeout(3600)&&!validTimeout(0)&&!validTimeout(65535));
  assert(!setupComplete(true,false)&&!setupComplete(false,true)&&setupComplete(true,true));
  assert(!std::strcmp(uvLevel(0),"Low"));assert(!std::strcmp(uvLevel(2),"Low"));
  assert(!std::strcmp(uvLevel(3),"Moderate"));assert(!std::strcmp(uvLevel(5),"Moderate"));
  assert(!std::strcmp(uvLevel(6),"High"));assert(!std::strcmp(uvLevel(8),"Very High"));assert(!std::strcmp(uvLevel(11),"Extreme"));
  assert(!std::strcmp(uvLevel(std::numeric_limits<float>::quiet_NaN()),"Unavailable"));
  const auto layout=statusLayout(20,28,30);assert(layout.rf>167&&layout.wifi>layout.rf+20&&layout.aprs>layout.wifi+28);
  assert(layout.wifi_left<layout.wifi&&layout.wifi_right<layout.aprs&&layout.aprs+30==310);
  assert(forecast_icon::classify("Partly Cloudy And Breezy")==forecast_icon::Kind::PartlyCloudy);
  assert(forecast_icon::classify("Mostly Cloudy And Windy")==forecast_icon::Kind::Cloudy);
  assert(forecast_icon::classify("Cloudy With Thunderstorms")==forecast_icon::Kind::Thunder);
  assert(!display_themes::find("unknown")&&!display_themes::find(nullptr));
  assert(std::strcmp(display_themes::get("unknown").id,"midnight")==0);
  auto luminance=[](uint16_t color){
    auto channel=[](double c){return c<=0.04045?c/12.92:std::pow((c+0.055)/1.055,2.4);};
    return 0.2126*channel(((color>>11)&31)/31.0)+0.7152*channel(((color>>5)&63)/63.0)+0.0722*channel((color&31)/31.0);
  };
  auto contrast=[&](uint16_t a,uint16_t b){double x=luminance(a),y=luminance(b);return (std::max(x,y)+0.05)/(std::min(x,y)+0.05);};
  unsigned theme_count=0;
  for(const auto& theme:display_themes::palettes){
    ++theme_count;assert(display_themes::find(theme.id)==&theme);
    assert(contrast(theme.text,theme.background)>=4.5&&contrast(theme.text,theme.card)>=4.5);
    assert(contrast(theme.muted,theme.background)>=4.5&&contrast(theme.muted,theme.card)>=4.5);
    assert(contrast(theme.accent,theme.background)>=3&&contrast(theme.connected,theme.alert_card)>=3);
    assert(contrast(theme.stale,theme.background)>=3);
  }
  assert(theme_count==7);
  const auto& night=display_themes::get("night");
  assert(night.brightness==24&&night.background==0);
  assert(luminance(night.text)<luminance(display_themes::get("midnight").text)/3);
  assert(luminance(night.connected)<luminance(display_themes::get("midnight").connected)/2);
  assert(luminance(moon_icon::nightTint(moon_icon::gray(242)))<luminance(moon_icon::gray(242))/3);
  Idle night_idle;night_idle.interact(1000);
  assert(night_idle.brightness(1001,"never",30,night.brightness)==24);
  assert(night_idle.brightness(30999,"dim",30,night.brightness)==24);
  assert(night_idle.brightness(31000,"dim",30,night.brightness)==2);
  assert(night_idle.brightness(31000,"off",30,night.brightness)==0);
  night_idle.interact(32000);assert(night_idle.brightness(32000,"dim",30,night.brightness)==24);
  for(const auto& theme:display_themes::palettes)if(std::strcmp(theme.id,"night"))assert(theme.brightness==160);
  const auto full=lunar_phase::advance(50,false,lunar_phase::kCycleSeconds/4);
  assert(std::abs(full.illumination-100)<0.001);
  const auto last_quarter=lunar_phase::advance(100,false,lunar_phase::kCycleSeconds/4);
  assert(std::abs(last_quarter.illumination-50)<0.001&&last_quarter.waning);
  const auto new_moon=lunar_phase::advance(50,true,lunar_phase::kCycleSeconds/4);
  assert(new_moon.illumination<0.001);
  assert(lunar_phase::advance(-1,false,0).illumination<0);
  assert(lunar_phase::advance(50,false,9*86400).illumination<0);
  assert(lunar_phase::advance(std::numeric_limits<float>::quiet_NaN(),false,0).illumination<0);
  assert(lunar_phase::referenceNoon(0)==0);
  std::cout<<"Display checks passed: clock/date, idle brightness, 60-second return, touch reset, setup completion, UV levels and cloudy conditions.\n";
}
