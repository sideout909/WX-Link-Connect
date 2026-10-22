#include "aprs_weather.h"
#include "daily_rain.h"
#include "nws_logic.h"
#include "rolling_rain.h"
#include "forecast_icon.h"
#include "aprs_login.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <ctime>
int main(){
  using Reply=aprs_login::Reply;
  assert(aprs_login::parse("# logresp N0CALL-13 verified, server TEST","N0CALL-13")==Reply::Verified);
  assert(aprs_login::parse("# logresp N0CALL-13 unverified, server TEST","N0CALL-13")==Reply::Unverified);
  assert(aprs_login::parse("# logresp OTHER verified, server TEST","N0CALL-13")==Reply::Other);
  assert(aprs_login::parse("# logresp N0CALL-13 not_verified","N0CALL-13")==Reply::Other);
  assert(aprs_login::parse("# logresp N0CALL-13 verified"+std::string(512,'x'),"N0CALL-13")==Reply::Other);
  assert(nws::isoTime("2026-10-18T00:00:00Z")==1792281600);
  assert(nws::isoTime("2026-10-17T20:00:00-04:00")==1792281600);
  assert(nws::isoTime("2026-10-18T01:30:00.123+01:30")==1792281600);
  assert(nws::isoTime("2026-02-29T00:00:00Z")==0);
  assert(nws::isoTime("2024-02-29T00:00:00Z")>0);
  assert(nws::isoTime("2026-10-18T00:00:00")==0);
  assert(nws::alertRank("Tornado Warning")>nws::alertRank("Severe Thunderstorm Warning"));
  assert(nws::alertRank("Flood Warning")>nws::alertRank("Tornado Watch"));
  std::vector<weather::RainInterval> intervals(weather::kRainIntervals);
  weather::RollingRain rolling(intervals.data());const uint64_t base=1792281600000ULL;
  rolling.record(base,0,0,false,false);
  assert(rolling.window(base,weather::kRainDayMs).valid&&!rolling.window(base,weather::kRainDayMs).complete);
  for(uint64_t elapsed=30000;elapsed<=weather::kRainDayMs;elapsed+=30000)rolling.record(base+elapsed,30000,elapsed==30000?2:0,true,false);
  auto full=rolling.window(base+weather::kRainDayMs,weather::kRainDayMs);assert(full.complete&&full.tips==2);
  auto boundary=rolling.window(base+weather::kRainDayMs+15000,weather::kRainDayMs);assert(!boundary.complete&&boundary.tips==0);
  rolling.record(base+weather::kRainDayMs+30000,30000,0,true,false);
  auto expired=rolling.window(base+weather::kRainDayMs+30000,weather::kRainDayMs);assert(expired.complete&&expired.tips==0);
  rolling.record(base+weather::kRainDayMs+210000,180000,3,false,false);
  auto gap=rolling.window(base+weather::kRainDayMs+210000,weather::kRainDayMs);assert(!gap.complete&&gap.tips==3);
  assert(!rolling.window(base+3*weather::kRainDayMs,weather::kRainDayMs).valid);
  assert(forecast_icon::classify("Slight Chance Showers And Thunderstorms")==forecast_icon::Kind::Thunder);
  assert(forecast_icon::classify("Mostly Sunny")==forecast_icon::Kind::PartlyCloudy);
  assert(forecast_icon::classify("Clear")==forecast_icon::Kind::Clear);
  assert(forecast_icon::illuminated(50,5,10,false,false));
  assert(!forecast_icon::illuminated(50,-5,10,false,false));
  assert(forecast_icon::illuminated(50,-5,10,true,false));
  aprs::Weather w;w.epoch=1792497600;w.latitude=42.999999;w.longitude=-83.5;
  w.temperature_valid=true;w.temperature_f=-9;w.humidity_valid=true;w.humidity=100;w.gust_valid=true;w.gust_mph=25.4;
  auto p=aprs::packet("N0CALL-13",w);
  assert(p.find("4300.00N/08330.00W_.../...g025t-09h00")!=std::string::npos);
  assert(p.find("r000")==std::string::npos);assert(p.find("b00000")==std::string::npos);
  assert(p.size()<512 && p.substr(p.size()-2)=="\r\n");
  assert(aprs::packet("BAD\r\nCALL",w).empty());
  w.rain_today_valid=true;w.rain_today=.49f;assert(aprs::packet("N0CALL",w).find("P049")!=std::string::npos);
  w.latitude=91;assert(aprs::packet("N0CALL",w).empty());
  assert(!aprs::identity("AB"));assert(!aprs::identity("N0CALL--1"));assert(!aprs::identity("N0CALL_1"));
  weather::DailyRain r;r.observe(20261020,1000000,0,false);assert(r.valid&&!r.complete&&r.tips==0);
  r.observe(20261020,1030000,2,true);assert(r.tips==2&&!r.complete);
  auto midnight=r.today(20261021,1045000);assert(midnight.valid&&midnight.tips==0&&!midnight.complete);
  assert(r.day==20261020&&r.tips==2); // A display refresh preserves the last packet's day/baseline.
  assert(!r.today(20261019,1045000).valid);assert(!r.today(20261020,1029999).valid);
  r.observe(20261021,1060000,0,true);assert(r.tips==0&&r.complete);
  r.observe(20261021,1090000,3,true);assert(r.tips==3&&r.complete);
  assert(r.today(20261021,1090000).complete);
  assert(!r.today(20261021,1210000).complete); // Staleness independently removes complete coverage.
  r.observe(20261022,1120000,1,true);assert(r.tips==1&&!r.complete); // Rain straddles midnight.
  r.observe(20261023,2000000,0,false);assert(!r.complete&&r.tips==0);
  assert(!weather::DailyRain{}.today(20261023,2000000).valid);
  // The configured local date, not a fixed 24-hour duration, defines today.
  setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1);tzset();
  auto epoch=[](int month,int day,int hour){std::tm t{};t.tm_year=126;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_isdst=-1;return uint64_t(std::mktime(&t))*1000;};
  auto date=[](uint64_t now){std::time_t seconds=now/1000;std::tm t{};localtime_r(&seconds,&t);return (t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday;};
  const auto fall=epoch(11,1,0),fall_end=epoch(11,2,0);
  assert(fall_end-fall==25ULL*3600000);
  weather::DailyRain fall_rain;fall_rain.observe(date(fall),fall,4,false);
  assert(fall_rain.today(date(fall_end-1),fall_end-1).tips==4);
  assert(fall_rain.today(date(fall_end),fall_end).tips==0);
  const auto spring=epoch(3,8,0),spring_end=epoch(3,9,0);
  assert(spring_end-spring==23ULL*3600000);
  weather::DailyRain spring_rain;spring_rain.observe(date(spring),spring,3,false);
  assert(spring_rain.today(date(spring_end-1),spring_end-1).tips==3);
  assert(spring_rain.today(date(spring_end),spring_end).tips==0);
  std::cout<<"Reporting checks passed: APRS missing fields, position rounding, negative temperature, humidity 100%, rain units, identity injection, midnight and gaps.\n";
}
