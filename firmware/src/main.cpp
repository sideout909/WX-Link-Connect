#include <Arduino.h>
#include <ArduinoJson.h>
#include <PanelLan.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <time.h>
#include "weather_store.h"
#include "atlas_packet.h"
#include "discovery_packet.h"
#include "forecast_icon.h"
#include "moon_icon.h"
#include "nws_pressure.h"
#include "theme_icons.h"
#include "display_themes.h"
#include "lunar_phase.h"
#include "home_fonts.h"
#include "daily_rain.h"
#include "checkpoint.h"
#include "archive_storage.h"
#include "services.h"
#include "radio_receiver.h"
#include "sensor_health.h"
#include "firmware_version.h"
#include "display_preferences.h"
#include "capture_session.h"
#include "capture_jsonl.h"
#include "connect_decoder_profile.h"
#include "capture_devices.h"
#include "preview_adapter.h"
#include "capture_pulse.h"
#include "capture_storage.h"
#include "capture_lease.h"
#include "capture_writer.h"
#include "setup_page.h"
#include "connect_web_security.h"
#include "connect_migration.h"

// JSON commands, HTTP handlers and display formatting share the main task.
// Keep explicit stack headroom; the Arduino default is only 8 KiB.
SET_LOOP_TASK_STACK_SIZE(24 * 1024);

namespace {
constexpr const char* kVersion = firmware_version::display;
constexpr uint16_t rgb565(unsigned r,unsigned g,unsigned b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
uint16_t kWhite=rgb565(240,245,251),kAmber=0xfd20;
const char* active_theme="midnight";
uint16_t kBackground,kCard,kMuted,kCyan,kDivider,kGreen,kAlertCard,kAlertBorder;
int8_t theme_preview=-1;
void palette(unsigned theme){
  kWhite=rgb565(240,245,251);kAmber=0xfd20;active_theme="midnight";
  if(theme==3){kBackground=rgb565(152,31,50);kCard=rgb565(125,25,43);kMuted=rgb565(255,225,229);kCyan=rgb565(184,241,255);kDivider=rgb565(195,111,123);kGreen=rgb565(196,255,214);kAlertCard=rgb565(98,18,35);kAlertBorder=rgb565(209,131,144);}
  else if(theme==2){kBackground=rgb565(148,64,0);kCard=rgb565(121,52,0);kMuted=rgb565(255,229,207);kCyan=rgb565(184,241,255);kDivider=rgb565(188,121,80);kGreen=rgb565(196,255,214);kAlertCard=rgb565(98,42,0);kAlertBorder=rgb565(207,139,83);}
  else{
    const auto& colors=display_themes::get(services::settings().display_theme.c_str());
    active_theme=colors.id;kBackground=colors.background;kCard=colors.card;kWhite=colors.text;
    kMuted=colors.muted;kCyan=colors.accent;kDivider=colors.divider;kGreen=colors.connected;
    kAlertCard=colors.alert_card;kAlertBorder=colors.alert_border;kAmber=colors.stale;
  }
}
PanelLan display(BOARD_SC05_X);
WebServer web_server(80);
LGFX_Sprite canvas(&display);
weather::Bucket* history = nullptr;
weather::RainInterval* rain_history=nullptr;
weather::Store* store = nullptr;
bool display_ok = false, replay = false, dirty = true;
uint32_t rf_stream_until=0;
uint8_t page = 0, metric = 0, forecast_index=0, alert_index=0;
int8_t measurement_detail=-1;
bool forecast_hourly=false;
bool setup_open=false,alerts_open=false,clock_anchored=false,checkpoint_restored=false;
wxconnect::capture::ClockMap capture_clock;
wxconnect::capture::CaptureSession capture_session;
wxconnect::capture::CandidateTable capture_candidates;
wxconnect::preview::Adapter preview_adapter;
bool preview_explicit=false;
using CaptureOwner=wxconnect::capture::CaptureLease::Owner;
wxconnect::capture::CaptureLease capture_lease;
char capture_json_output[wxconnect::capture::kWorstCaseEscapedNativeBytes+512];
char radio_packet_copy[radio_receiver::kDecodedBufferBytes];
char capture_read_buffer[16384];
// These large records are reused by the single Arduino loop task. Keeping them
// static avoids multi-kilobyte automatic frames in radio and web call chains.
wxconnect::capture::Record capture_record_scratch;
wxconnect::capture::PulseFrame capture_pulse_scratch;
bool setup_recovery=false;
display_preferences::Idle display_idle;
uint8_t display_brightness=160;
int detail_scroll=0;
uint64_t clock_epoch=0,clock_uptime=0,last_publish=0,last_checkpoint=0;
uint64_t capture_started_at_ms=0;
const char* capture_stop_reason="none";
uint32_t receiver_drops_at_start=0,raw_drops_at_start=0,raw_skips_at_start=0;
char csrf_token[33]{};
weather::DailyRain daily_rain;
sensor_health::Monitor sensor_monitor;
services::State network;
uint64_t source_time = 0, last_receive = 0, last_draw = 0, last_status = 0;
char line[1024]; size_t line_size = 0; bool discard_line = false;
uint32_t touch_events = 0, serial_errors = 0, request_id = 0;
uint32_t last_sample_request_id = 0;
unsigned last_sample_result = 0;
bool have_sample_reply = false;
uint64_t uptimeMs() { return uint64_t(esp_timer_get_time()) / 1000; }
void liveReceive();
uint64_t liveNow(){return clock_anchored ? clock_epoch+uptimeMs()-clock_uptime : uptimeMs()+1;}
uint64_t dataNow() { return replay && source_time ? source_time + uptimeMs() - last_receive : liveNow(); }
int32_t localDay(uint64_t now){time_t seconds=now/1000;struct tm t{};localtime_r(&seconds,&t);return (t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday;}
weather::DailyRain::Total todayRain(uint64_t now){return replay||clock_anchored?daily_rain.today(localDay(now),now):weather::DailyRain::Total{};}
sensor_health::State sensorState(){return sensor_monitor.state(replay||services::settings().radio_enabled,replay||radio_receiver::ready(),dataNow(),*store);}

void text(const String& value, int x, int y, uint16_t color = kWhite, const lgfx::IFont* font = &fonts::Font2) {
  canvas.setFont(font); canvas.setTextColor(color); canvas.drawString(value, x, y);
}
void card(int x, int y, int w, int h) { canvas.fillRoundRect(x,y,w,h,7,kCard); }
// The study uses DejaVu, baseline coordinates, and proportional spacing.
// Four-bit glyph coverage keeps those fonts readable at the native 320x240 size.
uint16_t nextCode(const char*& p){
  uint8_t c=uint8_t(*p++);if(c<128)return c;
  if((c&224)==192 && *p){uint16_t code=(c&31)<<6;return code|(uint8_t(*p++)&63);}
  if((c&240)==224 && p[0] && p[1]){uint16_t code=(c&15)<<12;code|=(uint8_t(*p++)&63)<<6;return code|(uint8_t(*p++)&63);}
  return '?';
}
const home_fonts::Glyph* homeGlyph(const home_fonts::Font& font,uint16_t code){
  for(unsigned i=0;i<font.count;++i)if(font.glyphs[i].code==code)return &font.glyphs[i];
  return nullptr;
}
int homeWidth(const String& value,const home_fonts::Font& font,int tracking=0){
  int pen=0;const char* p=value.c_str();while(*p){const auto* g=homeGlyph(font,nextCode(p));if(g)pen+=g->advance+tracking*64;}
  return (pen+32)/64;
}
void homeText(const String& value,int x,int baseline,uint16_t color,const home_fonts::Font& font,uint16_t background,int tracking=0){
  uint16_t shades[16];
  for(unsigned a=0;a<16;++a){
    const unsigned r=(((color>>11)&31)*a+((background>>11)&31)*(15-a)+7)/15;
    const unsigned g=(((color>>5)&63)*a+((background>>5)&63)*(15-a)+7)/15;
    const unsigned b=((color&31)*a+(background&31)*(15-a)+7)/15;
    shades[a]=(r<<11)|(g<<5)|b;
  }
  int pen=x*64;const char* p=value.c_str();
  while(*p){
    const auto* g=homeGlyph(font,nextCode(p));if(!g)continue;
    const int left=(pen+32)/64+g->left,top=baseline+g->top;
    for(unsigned y=0;y<g->height;++y)for(unsigned x=0;x<g->width;++x){
      unsigned i=y*g->width+x;uint8_t packed=font.pixels[g->offset+i/2],a=i%2?packed&15:packed>>4;
      if(a)canvas.drawPixel(left+x,top+y,shades[a]);
    }
    pen+=g->advance+tracking*64;
  }
}
void homeText(const String& value,int x,int baseline,uint16_t color,const home_fonts::Font& font,int tracking=0){homeText(value,x,baseline,color,font,kBackground,tracking);}
String homeFit(String value,int width,const home_fonts::Font& font){
  if(homeWidth(value,font)<=width)return value;
  while(value.length() && homeWidth(value+"...",font)>width){
    unsigned last=value.length()-1;while(last && (uint8_t(value[last])&192)==128)--last;value.remove(last);
  }
  return value+"...";
}
template <typename T> String value(const weather::Field<T>& f, unsigned digits = 0) {
  return f.valid ? String(f.value, digits) : String("--");
}
// Integer fields use a separate conversion; String(integer, n) means base n.
String integer(const weather::Field<uint16_t>& f) { return f.valid ? String(f.value) : String("--"); }
String luxText() { return store->lux.valid ? String(store->lux.value) : String("--"); }
String replayClock() {
  if (!replay && !clock_anchored) return "--:--";
  time_t seconds = time_t(dataNow()/1000);
  struct tm local{};localtime_r(&seconds,&local);
  char clock[40];display_preferences::headerText(clock,sizeof(clock),local,services::settings().date_format.c_str(),services::settings().clock_24h);
  return String(clock);
}
display_preferences::StatusLayout statusLayout(){
  const String link=String(services::settings().aprs_profile=="cwop"?"CWOP":"APRS")+" +";
  return display_preferences::statusLayout(homeWidth("RF +",home_fonts::status),homeWidth("WiFi +",home_fonts::status),homeWidth(link,home_fonts::status));
}
uint8_t normalBrightness(){return display_themes::get(services::settings().display_theme.c_str()).brightness;}
void interact(){display_idle.interact(uptimeMs());const uint8_t normal=normalBrightness();if(display_brightness!=normal){display_brightness=normal;display.setBrightness(normal);dirty=true;}}
void sparkline() {
  // Restored history is usable before a new live packet arrives.
  const uint64_t end=dataNow()/60000,begin=end>=59?end-59:0;
  float low=INFINITY,high=-INFINITY;unsigned count=0;
  for(uint64_t m=begin;m<=end;++m){
    const auto* b=store->bucket(m);
    if(b && b->temperature_count){const float v=b->temperature_sum/b->temperature_count;low=fminf(low,v);high=fmaxf(high,v);++count;}
  }
  if(count<2)return;
  int previous_x=0,previous_y=0;bool previous=false;
  for(uint64_t m=begin;m<=end;++m){
    const auto* b=store->bucket(m);
    if(!b || !b->temperature_count)continue; // Connect observed points across missed minutes.
    const float v=b->temperature_sum/b->temperature_count;
    const int x=162-int(end-m)*150/59,y=118-int((v-low)/(high>low?high-low:1)*10);
    if(previous)canvas.drawLine(previous_x,previous_y,x,y,kCyan);
    else canvas.drawPixel(x,y,kCyan); // An isolated observed minute is still data.
    previous_x=x;previous_y=y;previous=true;
  }
}
void weatherGraphic(forecast_icon::Kind kind,bool daytime,int x,int y,int size,bool current_moon=false,time_t lunar_time=0){
  const float scale=float(size)/48;
  auto png=[&](const char* name,int xx,int yy,float factor){const auto* icon=theme_icons::find(active_theme,name);if(icon)canvas.drawPng(icon->bytes,icon->size,xx,yy,0,0,0,0,factor,factor);};
  const bool sky=kind==forecast_icon::Kind::Clear||kind==forecast_icon::Kind::PartlyCloudy||kind==forecast_icon::Kind::Cloudy;
  if(kind!=forecast_icon::Kind::Unknown&&!daytime){
    const bool known=network.sun_day==localDay(dataNow());
    float illumination=known?network.moon_illumination:-1;
    bool waning=network.moon_phase.startsWith("Waning")||network.moon_phase=="Last Quarter";
    if(!current_moon){
      const time_t reference=lunar_phase::referenceNoon(network.sun_day);
      const auto phase=reference&&lunar_time?lunar_phase::advance(illumination,waning,double(lunar_time)-double(reference)):lunar_phase::Phase{};
      illumination=phase.illumination;waning=phase.waning;
    }
    const int cx=x+(sky?size/2:size*31/100),cy=y+(sky?size/2:size*29/100),r=size*(sky?31:23)/100;
    if(illumination<0){canvas.drawCircle(cx,cy,r,kMuted);text("?",cx-3,cy-4,kMuted,&fonts::Font0);}
    else moon_icon::draw(canvas,cx,cy,r,illumination,waning,services::settings().latitude<0,!std::strcmp(active_theme,"night"));
    if(sky){
      if(kind!=forecast_icon::Kind::Clear)png("cloudy",x+size/5,y+size/4,scale*.8f);
    }else weatherGraphic(kind,true,x+size/6,y+size/5,size*83/100);
    return;
  }
  using K=forecast_icon::Kind;
  switch(kind){
    case K::Clear:png("sunny",x,y,scale);break;
    case K::PartlyCloudy:png("partly-cloudy",x,y,scale);break;
    case K::Cloudy:
      png("sunny",x,y,scale*.8f);
      png("cloudy",x,y+size/8,scale);break;
    case K::Rain:png("rain",x,y,scale);break;
    case K::Thunder:png("thunderstorm",x,y,scale);break;
    case K::Snow:png("snow",x,y,scale);break;
    case K::Fog:png("fog",x,y,scale);break;
    case K::Wind:png("windy",x,y,scale);break;
    default:png("unknown",x,y,scale);break;
  }
}
bool forecastGraphic(){
  time_t now=time(nullptr);
  if(!network.wifi||!services::clockValid()||!network.hourly_fetched||now<network.hourly_fetched||now-network.hourly_fetched>=7200||network.hourly_error!="Current")return false;
  const services::ForecastPeriod* period=nullptr;
  for(unsigned i=0;i<network.hourly_count;++i){const auto& p=network.hourly[i];if(p.start<=now&&p.end>now){period=&p;break;}}
  if(!period)return false;
  bool daytime=period->is_daytime;
  const bool solar_current=network.sun_day==localDay(uint64_t(now)*1000);
  if(solar_current&&network.sun_status=="midnight_sun")daytime=true;
  else if(solar_current&&network.sun_status=="polar_night")daytime=false;
  else if(solar_current&&network.sunrise&&network.sunset)daytime=now>=network.sunrise&&now<network.sunset;
  else if(!period->daytime_known)return false;
  homeText("NWS",140,62,kMuted,home_fonts::small);
  weatherGraphic(forecast_icon::classify(period->short_text.c_str()),daytime,132,66,36,true);
  return true;
}
void renderHome() {
  using namespace home_fonts;
  const uint64_t now=dataNow();
  homeText("OUTDOOR °F",11,60,kMuted,label,1);
  const String degrees=value(store->temperature,1);
  const bool compact_temperature=homeWidth(degrees,temperature,-2)>123;
  const auto sensor_state=sensorState();
  const bool temperature_stale=store->temperature.valid&&
    (!store->temperature.fresh(now,sensor_health::kDataStaleMs)||sensor_state==sensor_health::State::DataStale||sensor_state==sensor_health::State::NoSignal);
  homeText(degrees,8,104,temperature_stale?kAmber:store->temperature.valid?kWhite:kMuted,compact_temperature?temperature_compact:temperature,-2);
  forecastGraphic();
  sparkline();
  const bool light_fresh=store->lux.fresh(now,sensor_health::kDataStaleMs),uv_fresh=store->uv.fresh(now,sensor_health::kDataStaleMs);
  const String light=store->lux.valid ? String(store->lux.value/1000.0f,1)+" klx"+(light_fresh?"":" !") : "-- klx";
  const String uv=uv_fresh?"UV "+String(store->uv.value,0)+" "+display_preferences::uvLevel(store->uv.value):"UV --";
  const auto& light_font=homeWidth(light+" · "+uv,body)<=164?body:unit;
  homeText(homeFit(light+" · "+uv,164,light_font),11,132,kMuted,light_font);
  canvas.drawFastVLine(180,55,77,kDivider);
  homeText("WIND",193,60,kMuted,label,1);
  constexpr int cx=209,cy=89,r=20;
  canvas.drawCircle(cx,cy,r,kDivider);homeText("N",206,72,kMuted,small);
  String bearing="--";
  if(store->direction.valid){
    constexpr float pi=3.14159265358979323846f;
    const float angle=store->direction.value*pi/180;
    const int dx=int(sinf(angle)*14),dy=int(-cosf(angle)*14);
    const uint16_t arrow=store->direction.fresh(now,sensor_health::kDataStaleMs)?kCyan:kMuted;
    canvas.drawLine(cx-dx/2,cy-dy/2,cx+dx,cy+dy,arrow);
    canvas.drawLine(cx-dx/2+1,cy-dy/2,cx+dx+1,cy+dy,arrow);
    canvas.fillTriangle(cx+dx,cy+dy,cx+dx/2-dy/3,cy+dy/2+dx/3,cx+dx/2+dy/3,cy+dy/2-dx/3,arrow);
    const char* bearings[]={"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
    bearing=store->direction.fresh(now,sensor_health::kDataStaleMs)?bearings[int((store->direction.value+11.25f)/22.5f)%16]:"STALE";
  }
  const String speed=value(store->wind,1);
  homeText(speed,238,98,store->wind.fresh(now,sensor_health::kDataStaleMs)?kWhite:kMuted,homeWidth(speed,wind)<=76?wind:reading);
  homeText(homeFit("mph · "+bearing,77,body),239,112,kMuted,body);
  const auto peak=store->gust(now);
  homeText(homeFit("Gust "+value(peak)+(store->accepted && !store->gustComplete(now)?"*":"")+" · 5 min",119,body),193,130,kMuted,body);
  if(store->wind.valid && !store->wind.fresh(now,sensor_health::kDataStaleMs))homeText("STALE",273,121,kAmber,small);
  canvas.fillRoundRect(8,140,96,43,5,kCard);canvas.fillRoundRect(112,140,96,43,5,kCard);canvas.fillRoundRect(216,140,96,43,5,kCard);
  homeText(store->humidity.valid&&!store->humidity.fresh(now,sensor_health::kDataStaleMs)?"HUMIDITY !":"HUMIDITY",15,153,kMuted,tile_label,kCard);
  const String humidity=integer(store->humidity);
  homeText(humidity,15,175,store->humidity.fresh(now,sensor_health::kDataStaleMs)?kWhite:kMuted,reading,kCard);
  homeText("%",15+homeWidth(humidity,reading)+6,175,kMuted,unit,kCard);
  const auto rain=store->rainState();const bool rain_fresh=rain.baseline&&now>=rain.time&&now-rain.time<sensor_health::kDataStaleMs;
  const auto rain_today=todayRain(now);
  homeText(!rain_today.valid?"RAIN TODAY":!rain_fresh?"RAIN TODAY !":rain_today.complete?"RAIN TODAY":"RAIN TODAY*",119,153,kMuted,tile_label,kCard);
  const String rain_amount=rain_today.valid?String(rain_today.tips/100.0f,2):"--";
  homeText(rain_amount,119,175,rain_fresh?kWhite:kMuted,reading,kCard);
  homeText("in",119+homeWidth(rain_amount,reading)+5,175,kMuted,unit,kCard);
  homeText("BARO",223,153,kMuted,tile_label,kCard);
  const bool pressure_fresh=network.wifi&&services::clockValid()&&nws_pressure::fresh(network.pressure_observed,time(nullptr));
  homeText(pressure_fresh?String(network.pressure_pa/3386.389f,2):"--",223,171,pressure_fresh?kWhite:kMuted,reading,kCard);
  if(pressure_fresh)homeText("inHg",284,170,kMuted,unit,kCard);
  homeText(pressure_fresh?"NWS":"NWS --",223,182,kMuted,home_fonts::status,kCard);
}

bool bucketValue(const weather::Bucket& b, float& out) {
  switch (metric) {
    case 0: if (b.temperature_count) { out=b.temperature_sum/b.temperature_count;return true;} break;
    case 1: if (b.wind_count) { out=b.wind_max;return true;} break;
    case 2: if (b.lux_count) { out=float(b.lux_sum)/b.lux_count;return true;} break;
    case 3: if (b.humidity_count) { out=float(b.humidity_sum)/b.humidity_count;return true;} break;
  }
  return false;
}
template<typename T> String fieldAge(const weather::Field<T>& field,uint64_t now,bool mark_stale=true){
  if(!field.valid)return "No reading";
  if(now<field.time_ms)return "Time unknown";
  const uint64_t seconds=(now-field.time_ms)/1000;
  return String(uint32_t(seconds<3600?seconds:seconds/60))+(seconds<3600?"s":" min")+(!mark_stale||field.fresh(now,sensor_health::kDataStaleMs)?"":" · STALE");
}
void detailRow(const String& label,const String& reading,int baseline){
  homeText(label,12,baseline,kMuted,home_fonts::body);
  const String fitted=homeFit(reading,177,home_fonts::clock);
  homeText(fitted,308-homeWidth(fitted,home_fonts::clock),baseline,kWhite,home_fonts::clock);
}
void renderMeasurement(){
  using namespace home_fonts;
  const char* titles[]={"OUTDOOR","WIND","HUMIDITY","RAIN","PRESSURE","LINK STATUS"};
  homeText("‹ Back",12,26,kCyan,header);homeText(titles[measurement_detail],89,26,kWhite,header);
  if(replay)homeText("REPLAY",263,15,kAmber,home_fonts::status);
  canvas.drawFastHLine(10,44,300,kDivider);
  const uint64_t now=dataNow();
  if(measurement_detail==0){
    detailRow("Temperature",value(store->temperature,1)+" °F",66);
    detailRow("Updated",fieldAge(store->temperature,now),88);
    detailRow("Local light",store->lux.valid?String(store->lux.value)+" lux":"--",110);
    detailRow("Light updated",fieldAge(store->lux,now),132);
    detailRow("UV index",value(store->uv,1),154);
    detailRow("UV updated",fieldAge(store->uv,now),176);
  }else if(measurement_detail==1){
    const auto peak=store->gust(now);
    detailRow("Current",value(store->wind,1)+" mph",66);
    detailRow("Direction",!store->direction.valid?"--":value(store->direction)+"°"+(store->direction.fresh(now,sensor_health::kDataStaleMs)?"":" · STALE"),88);
    detailRow("Gust · 5 min",value(peak,1)+" mph",110);
    detailRow("Peak age",fieldAge(peak,now,false),132);
    detailRow("Wind age",fieldAge(store->wind,now),154);
    detailRow("Coverage",!store->wind.valid?"No readings":store->gustComplete(now)?"Full 5 min":"Partial / gaps",176);
  }else if(measurement_detail==2){
    detailRow("Humidity",integer(store->humidity)+" %",66);
    detailRow("Updated",fieldAge(store->humidity,now),88);
    detailRow("Sensor",String(services::settings().station_id)+" / "+services::settings().channel,110);
    detailRow("Status",!store->humidity.valid?"No reading":store->humidity.fresh(now,sensor_health::kDataStaleMs)?"Fresh":"STALE",132);
    detailRow("Source",replay?"Recorded USB replay":store->accepted?"Atlas radio":"Waiting for Atlas",154);
    homeText("Swipe to open humidity trends",12,176,kMuted,body);
  }else if(measurement_detail==3){
    const auto today=todayRain(now);
    const auto hour=store->rain_timeline.window(now,3600000),day=store->rain_timeline.window(now,weather::kRainDayMs);
    auto amount=[](uint32_t tips,bool valid,bool complete){return valid?String(tips/100.0f,2)+" in"+(complete?"":" *"):String("--");};
    detailRow("Today",amount(today.tips,today.valid,today.complete),66);
    detailRow("Last hour",amount(hour.tips,hour.valid,hour.complete),88);
    detailRow("Last 24 hours",amount(day.tips,day.valid,day.complete),110);
    const auto counter=store->rainState();
    const String age=!counter.baseline?"No reading":now<counter.time?"Time unknown":String(uint32_t((now-counter.time)/1000))+"s"+(now-counter.time>=sensor_health::kDataStaleMs?" · STALE":"");
    detailRow("Counter age",age,132);
    detailRow("Today resets","00:00 local",154);
    homeText("* Partial or uncertain coverage",12,176,kMuted,body);
  }else if(measurement_detail==4){
    const bool fresh=network.wifi&&services::clockValid()&&nws_pressure::fresh(network.pressure_observed,time(nullptr));
    detailRow("Barometer",fresh?String(network.pressure_pa/3386.389f,2)+" inHg":"--",66);
    detailRow("Source",network.pressure_station.length()?"NWS / "+network.pressure_station:"NWS / pending",88);
    detailRow("Pressure type",network.pressure_sea_level?"Sea-level pressure":"Reported barometric",110);
    detailRow("Observed",network.pressure_observed?String(uint32_t(std::max(time_t(0),time(nullptr)-network.pressure_observed))/60)+" min ago":"No observation",132);
    homeText(homeFit(network.pressure_error,296,body),12,155,kMuted,body);
    homeText("Nearby station; Atlas has no barometer.",12,176,kMuted,home_fonts::status);
  }else{
    const auto state=sensorState();
    detailRow("Atlas",sensor_health::label(state),66);
    detailRow("Last packet",!sensor_monitor.received?"None":now<sensor_monitor.last_rx?"Time unknown":String(uint32_t((now-sensor_monitor.last_rx)/1000))+"s ago",88);
    String missing;for(unsigned i=0;i<3;++i)if(!sensor_monitor.types[i]||now<sensor_monitor.types[i]||now-sensor_monitor.types[i]>=sensor_health::kDataStaleMs){if(missing.length())missing+=" / ";missing+=String(37+i);}
    detailRow("Missing types",missing.length()?missing:"None",110);
    detailRow(services::settings().aprs_profile=="cwop"?"CWOP link":"APRS link",network.aprs_connected&&network.wifi?(network.aprs_verified?"Connected · verified":"Connected · CWOP"):!network.wifi&&services::settings().aprs_enabled?String("Wi-Fi unavailable"):network.aprs_state,132);
    detailRow("Last report",network.aprs_success&&services::clockValid()?String(uint32_t(std::max(time_t(0),time(nullptr)-network.aprs_success)))+"s ago":"None sent",154);
    homeText(state==sensor_health::State::DataStale?"No changes / incomplete data for 10 min.":state==sensor_health::State::NoSignal?"No packets for 5 min. Check Atlas power / range.":"APRS send is not an ingestion ACK.",12,176,kMuted,home_fonts::status);
  }
}
void renderTrends() {
  const char* titles[] = {"Temperature (F)","Wind peaks (mph)","Light (lux)","Humidity (%)"};
  text(titles[metric],12,50,kCyan);
  text("tap chart to change",170,53,kMuted,&fonts::Font0);
  const uint64_t end = dataNow()/60000, begin = end>=1439 ? end-1439 : 0;
  float low=INFINITY,high=-INFINITY;unsigned count=0;
  for (uint64_t m=begin;m<=end;++m) {
    float v;const auto* b=store->bucket(m);
    if (b && bucketValue(*b,v)) { low=fminf(low,v);high=fmaxf(high,v);++count; }
  }
  if (!count) { text("No samples for this metric",30,105,kMuted);return; }
  text(String(low,1)+" to "+String(high,1),12,72,kMuted,&fonts::Font0);
  const float range = high>low ? high-low : 1;
  canvas.drawFastHLine(16,161,288,kMuted);
  // One vertical min/max mark per screen column; empty columns stay blank.
  for (int x=0;x<288;++x) {
    // Keep "now" at the right edge even before UTC sync, when the uptime
    // minute index is smaller than the displayed 24-hour window.
    const int64_t first=int64_t(end)+1-1440+int64_t(x)*1440/288;
    const int64_t stop=int64_t(end)+1-1440+int64_t(x+1)*1440/288;
    float minv=INFINITY,maxv=-INFINITY;
    for (int64_t m=std::max(int64_t(0),first);m<stop && uint64_t(m)<=end;++m) {
      float v;const auto* b=store->bucket(m);
      if (b && bucketValue(*b,v)) { minv=fminf(minv,v);maxv=fmaxf(maxv,v); }
    }
    if (std::isfinite(minv)) {
      const int top=156-int((maxv-low)/range*67), bottom=156-int((minv-low)/range*67);
      canvas.drawFastVLine(16+x,top,max(1,bottom-top+1),kCyan);
    }
  }
  text("24 hours ago",16,165,kMuted,&fonts::Font0);
  text("now",282,165,kMuted,&fonts::Font0);
}
void wrapped(const String& text_value,int x,int y,int width,int height,int scroll=0) {
  canvas.setClipRect(x,y,width,height);canvas.setFont(&fonts::Font2);canvas.setTextColor(kWhite);
  String line;int yy=y-scroll;
  for(unsigned i=0;i<text_value.length();++i){
    char ch=text_value[i];
    if(ch=='\n' || canvas.textWidth(line+ch)>width){canvas.drawString(line,x,yy);yy+=17;line="";if(ch=='\n')continue;}
    line+=ch;
  }
  if(line.length())canvas.drawString(line,x,yy);canvas.clearClipRect();
}
void renderForecast() {
  const unsigned count=forecast_hourly?network.hourly_count:network.forecast_count;
  const String forecast_error=forecast_hourly?network.hourly_error:network.forecast_error;
  text(forecast_hourly?"Hourly / tap for daily":"Daily / tap for hourly",12,47,kCyan,&fonts::Font0);
  if(!count){text("Local forecast",16,68,kWhite,&fonts::Font4);wrapped(forecast_error,16,100,288,50);text("Tap title for Wi-Fi / location setup",16,157,kMuted,&fonts::Font0);return;}
  if(forecast_index>=count)forecast_index=0;
  const auto& f=forecast_hourly?network.hourly[forecast_index]:network.forecast[forecast_index];
  String period_name=f.name;if(forecast_hourly){struct tm local{};localtime_r(&f.start,&local);char day[8],clock[16];strftime(day,sizeof(day),"%a",&local);display_preferences::clockText(clock,sizeof(clock),local,services::settings().clock_24h);period_name=String(day)+" "+clock;}
  text(period_name.substring(0,22),12,64,kWhite);text(String(forecast_index+1)+"/"+String(count),282,47,kMuted,&fonts::Font0);
  text(f.temperature_valid?String(f.temperature)+" F":"-- F",12,87,kWhite,&fonts::Font4);
  text(f.rain_probability>=0?"Rain "+String(f.rain_probability)+"%":"Rain --",116,89,kMuted,&fonts::Font0);
  const bool fresh=forecast_error=="Current"&&network.wifi&&services::clockValid()&&time(nullptr)>=(forecast_hourly?network.hourly_fetched:network.forecast_fetched)&&time(nullptr)-(forecast_hourly?network.hourly_fetched:network.forecast_fetched)<7200;
  text(fresh?"NWS":"NWS / STALE",116,102,fresh?kMuted:kAmber,&fonts::Font0);
  weatherGraphic(forecast_icon::classify(f.short_text.c_str()),!f.daytime_known||f.is_daytime,260,64,48,false,f.end>f.start?f.start+(f.end-f.start)/2:time_t(0));
  wrapped(f.short_text+". "+f.wind+". "+f.detail,12,120,296,47,detail_scroll);
  time_t issued=forecast_hourly?network.hourly_issued:network.forecast_issued;time_t checked=forecast_hourly?network.hourly_fetched:network.forecast_fetched;
  struct tm issue_local{};localtime_r(&issued,&issue_local);char issue_text[12];strftime(issue_text,sizeof(issue_text),"%a %H:%M",&issue_local);
  text(String("Issued ")+(issued?issue_text:"--")+" / checked "+String(max(0L,long(time(nullptr)-checked))/60)+"m ago",12,174,kMuted,&fonts::Font0);
}
void renderSetup(){
  const auto& settings=services::settings();
  if(display_preferences::setupComplete(settings.ssid.length(),settings.station_confirmed)&&!setup_recovery){
    canvas.fillScreen(kBackground);text("MORE SETTINGS",12,9,kCyan,&fonts::Font4);
    text("Tap header to close",175,35,kMuted,&fonts::Font0);
    text(network.wifi?"On your home Wi-Fi, open:":"Use the setup network, then open:",12,54,kMuted);
    text(services::portalUrl(),12,78,kWhite,&fonts::Font4);
    text("IP: http://"+services::portalAddress(),12,108,kMuted,&fonts::Font0);
    text("Username: admin",12,130,kCyan);
    text(settings.admin_custom?"Password: your custom password":"Password:",12,153,kMuted);
    if(!settings.admin_custom)text(services::setupWifiKey(),12,174,kWhite);
    canvas.drawRoundRect(12,193,296,34,5,kDivider);text("Wi-Fi recovery / connection details",20,201,kCyan);
    return;
  }
  canvas.fillScreen(kBackground);text("SETUP",12,9,kCyan,&fonts::Font4);
  text("Tap header to close",175,16,kMuted,&fonts::Font0);
  text("Join Wi-Fi:",12,47,kMuted);text(services::setupSsid(),12,68,kWhite,&fonts::Font4);
  text("Setup Wi-Fi key:",12,102,kMuted);text(services::setupWifiKey(),12,124,kWhite,&fonts::Font4);
  text("Open "+services::portalUrl(),12,164,kCyan);
  text("Setup IP: http://192.168.4.1",12,185,kMuted,&fonts::Font0);
  text(services::settings().admin_custom?"Admin: custom password set":"Admin: use the Wi-Fi key",12,202,kMuted,&fonts::Font0);
  text(network.wifi?"Home IP: http://"+services::portalAddress():"If the name fails, use the setup IP.",12,219,kMuted,&fonts::Font0);
}
void renderAlerts(){
  canvas.fillScreen(kBackground);text(network.alerts_valid && services::clockValid() && time(nullptr)-network.alert_fetched<180?"NWS ALERTS":"NWS / UNVERIFIED",12,9,kCyan);text("tap header to close",190,13,kMuted,&fonts::Font0);
  if(!network.alert_count){wrapped(network.alerts_valid && time(nullptr)-network.alert_fetched<180?"No active alerts. Updated by NWS.":"Alert status unverified. "+network.alert_error,12,55,296,150);return;}
  alert_index%=network.alert_count;const auto& a=network.alerts[alert_index];
  text(String(alert_index+1)+"/"+String(network.alert_count)+" "+a.event.substring(0,33),12,38,kAmber,&fonts::Font0);
  wrapped(a.headline+"\n\n"+a.description+"\n"+a.instruction,12,57,296,149,detail_scroll);
  text("tap bottom: next / drag: scroll",12,221,kMuted,&fonts::Font0);
}
void draw() {
  if (!display_ok) return;
  network=services::state();
  palette(0);
  if(setup_open){renderSetup();canvas.pushSprite(0,0);dirty=false;last_draw=uptimeMs();return;}
  canvas.fillScreen(kBackground);
  const auto& preview=preview_adapter.model();
  const uint64_t now=uptimeMs();
  auto fresh=[&](const wxconnect::preview::Value& field){return field.valid&&now>=field.time_ms&&now-field.time_ms<60000;};
  const String device=preview.selected
      ? String(preview.identity.has_model?preview.identity.model.data():"Unknown decoder")+
          (preview.identity.has_id?" · "+String(preview.identity.id.data()):" · identity incomplete")
      : String("Waiting for a decoded device");
  text("WX CONNECT",10,7,kCyan,&fonts::Font2);
  text(radio_receiver::ready()?"RF +":"RF --",204,9,radio_receiver::ready()?kGreen:kAmber,&fonts::Font0);
  text(network.wifi?"WiFi +":"WiFi setup",247,9,network.wifi?kGreen:kMuted,&fonts::Font0);
  text(device.substring(0,42),10,26,preview.selected?kWhite:kMuted,&fonts::Font0);
  canvas.drawFastHLine(10,41,300,kDivider);

  const auto& temperature=preview.temperature_c;
  const String temperature_f=temperature.valid?String(temperature.value*9.0/5.0+32.0,1):String("--");
  text("OUTDOOR °F",10,50,kMuted,&fonts::Font0);
  text(temperature_f,8,65,fresh(temperature)?kWhite:temperature.valid?kAmber:kMuted,&fonts::Font4);
  const auto& wind=preview.wind_mph;
  const String wind_text=wind.valid?String(wind.value,1):String("--");
  text("WIND",191,50,kMuted,&fonts::Font0);
  text(wind_text,190,65,fresh(wind)?kWhite:wind.valid?kAmber:kMuted,&fonts::Font4);
  const String direction=preview.wind_direction_deg.valid
      ? String(preview.wind_direction_deg.value,0)+"°":"--";
  text("mph · "+direction,192,101,kMuted,&fonts::Font0);
  const auto& humidity=preview.humidity_pct;
  const String humidity_text=humidity.valid?String(humidity.value,0):String("--");
  const auto& rain=preview.rain_counter;
  const String rain_text=rain.valid?String(rain.value,2):String("--");
  const char* rain_unit=preview.rain_unit==wxconnect::preview::RainUnit::Inch?"in":
      preview.rain_unit==wxconnect::preview::RainUnit::Millimetre?"mm":"";
  const wxconnect::capture::CandidateIdentity* selected=nullptr;
  uint32_t selected_packets=0;
  for(const auto& candidate:capture_candidates.entries()){
    if(!candidate.occupied)continue;
    const auto& identity=preview.identity;
    if(candidate.has_protocol==identity.has_protocol&&candidate.has_model==identity.has_model&&
       candidate.has_id==identity.has_id&&candidate.has_channel==identity.has_channel&&
       (!candidate.has_protocol||candidate.protocol==identity.protocol)&&
       (!candidate.has_model||candidate.model==identity.model)&&
       (!candidate.has_id||candidate.id==identity.id)&&
       (!candidate.has_channel||candidate.channel==identity.channel)){selected=&candidate;selected_packets=candidate.packets;break;}
  }
  const String selected_packet_count=selected?String(selected_packets):String("--");
  card(8,120,96,50);card(112,120,96,50);card(216,120,96,50);
  text("HUMIDITY",15,126,kMuted,&fonts::Font0);text(humidity_text+"%",15,140,fresh(humidity)?kWhite:humidity.valid?kAmber:kMuted,&fonts::Font2);
  text("RAIN COUNTER",119,126,kMuted,&fonts::Font0);text(rain_text+(rain_unit[0]?" "+String(rain_unit):""),119,140,fresh(rain)?kWhite:rain.valid?kAmber:kMuted,&fonts::Font2);
  text("PACKETS",223,126,kMuted,&fonts::Font0);text(selected_packet_count,223,140,kWhite,&fonts::Font2);
  const auto& lux=preview.light_lux;const auto& uv=preview.uv_index;
  text("LIGHT "+(lux.valid?String(lux.value,0)+" lux":String("--")),10,178,fresh(lux)?kMuted:kAmber,&fonts::Font0);
  text("UV "+(uv.valid?String(uv.value,1):String("--")),166,178,fresh(uv)?kMuted:kAmber,&fonts::Font0);
  canvas.fillRoundRect(8,195,304,38,5,kCard);
  text(capture_session.active()?"RECORDING · CAPTURE ONLY":"READY · CAPTURE ONLY",15,200,capture_session.active()?kGreen:kCyan,&fonts::Font0);
  text("All devices recorded · Preview is independent",15,215,kMuted,&fonts::Font0);
  if(preview.selected&&!preview.adapter_available)text("No weather adapter · raw packets retained",15,185,kAmber,&fonts::Font0);
  (void)selected_packets;
  canvas.pushSprite(0,0);
  dirty=false;last_draw=uptimeMs();
}
void rfStatus(){
  const auto r=radio_receiver::diagnostics();StaticJsonDocument<1536> d;
  d["event"]="rf_status";d["request_id"]=request_id;d["version"]=kVersion;d["uptime_ms"]=millis();
  d["radio_ready"]=r.ready;d["receiver_enabled"]=r.enabled;d["source"]=replay?"usb_replay":"radio";
  d["current_rssi_dbm"]=r.current_rssi;d["average_rssi_dbm"]=r.average_rssi;d["threshold_dbm"]=r.threshold;
  d["capture_margin_db"]=r.margin;
  d["decoded_signals"]=r.decoded_signals;d["decoded_messages"]=r.decoded_messages;d["zero_decodes"]=r.zero_decodes;
  d["capture_drops"]=r.capture_drops;d["decoder_drops"]=r.decoder_drops;d["app_queue_drops"]=radio_receiver::dropped();
  d["sensor_status"]=sensor_health::label(sensorState());d["accepted"]=store->accepted;d["repeats"]=store->duplicates;d["rejected"]=store->rejected;
  const uint64_t now=dataNow();JsonArray types=d.createNestedArray("type_ages_ms");
  for(auto stamp:sensor_monitor.types){if(stamp&&now>=stamp)types.add(now-stamp);else types.add(nullptr);}
  JsonObject ages=d.createNestedObject("field_ages_ms");
  auto age=[&](const char* key,uint64_t stamp,bool valid){if(valid&&now>=stamp)ages[key]=now-stamp;else ages[key]=nullptr;};
  age("temperature",store->temperature.time_ms,store->temperature.valid);age("humidity",store->humidity.time_ms,store->humidity.valid);
  age("wind",store->wind.time_ms,store->wind.valid);age("direction",store->direction.time_ms,store->direction.valid);
  age("lux",store->lux.time_ms,store->lux.valid);age("uv",store->uv.time_ms,store->uv.valid);
  const auto rain=store->rainState();age("rain",rain.time,rain.baseline);
  char buffer[1536];const size_t length=serializeJson(d,buffer,sizeof(buffer));Serial.write(reinterpret_cast<const uint8_t*>(buffer),length);Serial.println();
}
void status(const char* event = "status") {
  StaticJsonDocument<3072> out;
  out["event"]=event;out["request_id"]=request_id;out["version"]=kVersion;out["board"]="panellan_sc05_x";
  out["display_initialized"]=display_ok;out["width"]=display.width();out["height"]=display.height();
  out["display_theme"]=services::settings().display_theme;out["display_brightness"]=display_brightness;out["display_idle_s"]=(uptimeMs()-display_idle.last_interaction)/1000;
  out["flash_bytes"]=ESP.getFlashChipSize();out["psram_bytes"]=ESP.getPsramSize();
  out["free_heap"]=ESP.getFreeHeap();out["loop_stack_free"]=uxTaskGetStackHighWaterMark(nullptr);out["free_psram"]=ESP.getFreePsram();
  out["uptime_ms"]=uptimeMs();out["source"]=replay?"usb_replay":store->accepted?"radio":"none";
  out["radio_ready"]=radio_receiver::ready();out["radio_dropped"]=radio_receiver::dropped();
  out["wifi"]=network.wifi;out["clock_valid"]=services::clockValid();out["setup_ip"]=services::portalAddress();out["local_url"]=services::portalUrl();out["mdns_ready"]=services::localNameReady();
  out["checkpoint_restored"]=checkpoint_restored;out["setup_open"]=setup_open;
  const auto history_state=store->historySummary(dataNow());
  out["history_buckets"]=history_state.buckets;
  if(history_state.buckets){out["history_first_minute"]=history_state.first_minute;out["history_last_minute"]=history_state.last_minute;}
  out["portal_tested"]=network.portal_tested;out["portal_passed"]=network.portal_passed;
  out["portal_attempts"]=network.portal_attempts;out["portal_test_status"]=network.portal_test_status;
  out["custom_admin_password"]=services::settings().admin_custom;
  out["setup_credentials_separated"]=services::settings().admin_custom && services::settings().setup_wifi_key!=services::settings().setup_key;
  out["rain_24h_in"]=store->rain_timeline.window(dataNow(),weather::kRainDayMs).tips/100.0f;
  const auto rain_today=todayRain(dataNow());
  if(rain_today.valid)out["rain_today_in"]=rain_today.tips/100.0f;else out["rain_today_in"]=nullptr;
  out["rain_today_complete"]=rain_today.complete;
  out["page"]=page;out["touch_events"]=touch_events;out["serial_errors"]=serial_errors;
  out["theme_preview"]=theme_preview;
  out["measurement_detail"]=measurement_detail;
  out["sensor_status"]=sensor_health::label(sensorState());out["bound_packets_heard"]=sensor_monitor.received;
  out["accepted"]=store->accepted;out["duplicates"]=store->duplicates;out["rejected"]=store->rejected;
  out["rain_session_in"]=store->session_rain_tips/100.0;out["rain_discontinuities"]=store->rain_discontinuities;
  auto peak=store->gust(dataNow());if(peak.valid)out["gust_5m_mph"]=peak.value;
  if(store->temperature.valid)out["temperature_F"]=store->temperature.value;
  if(store->lux.valid)out["lux"]=store->lux.value;
  char buffer[3072];
  const size_t length=serializeJson(out,buffer,sizeof(buffer));
  Serial.write(reinterpret_cast<const uint8_t*>(buffer),length);Serial.println();
}
bool writeCaptureRecord(const wxconnect::capture::Record& record) {
  const size_t used=wxconnect::capture::encodeJsonl(record,capture_json_output,sizeof(capture_json_output));
  if(!used||Serial.write(reinterpret_cast<const uint8_t*>(capture_json_output),used)!=used)
    capture_session.transportDrop();
  if(!used){
    capture_stop_reason="storage_write_failed";
    capture_session.storageDrop();capture_session.abortForStorageFailure();
    wxconnect::capture::storage::finish();capture_lease.clear();return false;
  }
  capture_stop_reason="none";
  return wxconnect::capture::appendOrStop(capture_session,record.event,capture_json_output,used,
      [](wxconnect::capture::Event event,const char* bytes,size_t length){
        return wxconnect::capture::storage::append(event,bytes,length);
      },[](){
        capture_stop_reason="storage_write_failed";
        wxconnect::capture::storage::finish();capture_lease.clear();
      });
}
void attachCaptureDiagnostics(const char* reason){
  capture_record_scratch.capture_counters=capture_session.counters();
  capture_record_scratch.receiver_drops=radio_receiver::dropped()-receiver_drops_at_start;
  capture_record_scratch.raw_callback_drops=radio_receiver::rawDropped()-raw_drops_at_start;
  capture_record_scratch.raw_callback_skips=radio_receiver::rawSkipped()-raw_skips_at_start;
  capture_record_scratch.capture_diagnostics_valid=true;
  std::strncpy(capture_record_scratch.stop_reason.data(),reason,capture_record_scratch.stop_reason.size()-1);
}
void drainPulseQueue(){
  while(radio_receiver::popPulse(capture_pulse_scratch)){
    if(capture_session.makePulseRecord(capture_pulse_scratch,millis(),capture_clock,capture_record_scratch))writeCaptureRecord(capture_record_scratch);
  }
}
void drainCaptureQueue() {
  while(capture_session.popPacket(capture_record_scratch,capture_clock))writeCaptureRecord(capture_record_scratch);
}
const wxconnect::capture::CandidateIdentity* soleCandidate();
bool selectPreview(const wxconnect::capture::CandidateIdentity& candidate);
void refreshAutomaticPreview();
void liveReceive();

bool nativeField(JsonVariantConst native,const char* key,char* output,size_t capacity) {
  output[0]='\0';
  JsonVariantConst value=native[key];
  if(value.is<const char*>()){
    const char* text=value.as<const char*>();
    const size_t length=strlen(text);
    if(length>=capacity)return false;
    memcpy(output,text,length+1);return true;
  }
  if(!value.is<int64_t>()&&!value.is<uint64_t>()&&!value.is<double>())return false;
  const size_t length=serializeJson(value,output,capacity);
  return length<capacity;
}

template <typename T> void load(JsonVariantConst v, weather::Field<T>& f) {
  if (v.is<T>()) {f.valid=true;f.value=v.as<T>();}
}
void resetData() {
  for (size_t i=0;i<weather::kHistoryBuckets;++i) history[i]=weather::Bucket{};
  for(size_t i=0;i<weather::kRainIntervals;++i)rain_history[i]=weather::RainInterval{};
  store->~Store(); new(store) weather::Store(history,services::settings().station_id,services::settings().channel,rain_history);
  daily_rain=weather::DailyRain{};have_sample_reply=false;
  sensor_monitor.reset(liveNow());
  replay=false;theme_preview=-1;measurement_detail=-1;source_time=last_receive=0;dirty=true;
  services::Observation cleared;services::publish(cleared);
}
void command(const char* input) {
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc,input)) { ++serial_errors;Serial.println("{\"event\":\"error\",\"reason\":\"invalid_json\"}");return; }
  request_id=doc["request_id"]|uint32_t(0);
  const char* op=doc["op"]|"";
  if (!strcmp(op,"capture_start")) {
    if(wxconnect::connect_migration::blocksCapture()||capture_session.active()||!radio_receiver::ready()){
      const char* reason=wxconnect::connect_migration::blocksCapture()?"firmware_transition_busy":capture_session.active()?"already_recording":"radio_unavailable";
      Serial.printf("{\"event\":\"capture_control\",\"request_id\":%u,\"ok\":false,\"reason\":\"%s\"}\n",request_id,reason);
    } else {
      if(services::clockValid())capture_clock.synchronize(millis(),uint64_t(time(nullptr))*1000);
      uint8_t random_bytes[16];
      for(size_t i=0;i<sizeof(random_bytes);i+=sizeof(uint32_t)){
        const uint32_t random_word=esp_random();memcpy(random_bytes+i,&random_word,sizeof(random_word));
      }
      char session_id[wxconnect::capture::kSessionIdChars+1];
      wxconnect::capture::formatSessionId(random_bytes,session_id);
      if(wxconnect::capture::storage::beginSession(session_id)&&
         capture_session.start(session_id,wxconnect::capture::Source::Live,wxconnect::radio_profile::kId,millis(),capture_clock,capture_record_scratch)){
        capture_lease.begin(CaptureOwner::Usb,uptimeMs());
        capture_stop_reason="none";
        receiver_drops_at_start=radio_receiver::dropped();raw_drops_at_start=radio_receiver::rawDropped();raw_skips_at_start=radio_receiver::rawSkipped();
        capture_started_at_ms=uptimeMs();
        const bool recorded=writeCaptureRecord(capture_record_scratch);
        Serial.printf("{\"event\":\"capture_control\",\"request_id\":%u,\"ok\":%s,\"session_id\":\"%s\",\"reason\":\"%s\"}\n",request_id,recorded?"true":"false",recorded?session_id:"",recorded?"none":"storage_write_failed");
      } else {
        if(wxconnect::capture::storage::state().active)wxconnect::capture::storage::finish();
        Serial.printf("{\"event\":\"capture_control\",\"request_id\":%u,\"ok\":false,\"reason\":\"storage_unavailable_or_invalid_session\"}\n",request_id);
      }
    }
  }
  else if (!strcmp(op,"capture_stop")) {
    if(capture_lease.owner()!=CaptureOwner::Usb){
      Serial.printf("{\"event\":\"capture_control\",\"request_id\":%u,\"ok\":false,\"reason\":\"not_usb_capture\"}\n",request_id);return;
    }
    radio_receiver::stopRawWindow();
    liveReceive();
    drainCaptureQueue();
    if(capture_session.stop(millis(),capture_clock,capture_record_scratch)){
      attachCaptureDiagnostics("requested");
      const bool saved=writeCaptureRecord(capture_record_scratch)&&wxconnect::capture::storage::finish();capture_lease.clear();
      Serial.printf("{\"event\":\"capture_control\",\"request_id\":%u,\"ok\":%s,\"storage_flushed\":%s}\n",request_id,saved?"true":"false",saved?"true":"false");
    } else Serial.printf("{\"event\":\"capture_control\",\"request_id\":%u,\"ok\":false,\"reason\":\"not_recording_or_queue_pending\"}\n",request_id);
  }
  else if (!strcmp(op,"status")) status();
  else if(!strcmp(op,"capture_keepalive")){
    const bool active=capture_session.active()&&capture_lease.heartbeat(CaptureOwner::Usb,uptimeMs());
    Serial.printf("{\"event\":\"capture_heartbeat\",\"request_id\":%u,\"active\":%s,\"session_id\":\"%s\"}\n",
        request_id,active?"true":"false",active?capture_session.sessionId():"");
  }
  else if(!strcmp(op,"capture_status")){
    const auto& counters=capture_session.counters();
    const auto storage=wxconnect::capture::storage::state();
  Serial.printf("{\"event\":\"capture_status\",\"request_id\":%u,\"active\":%s,\"session_id\":\"%s\",\"queued\":%u,\"packets\":%llu,\"pulses\":%llu,\"raw_window_active\":%s,\"raw_drops\":%u,\"raw_skips\":%u,\"oversize_drops\":%llu,\"invalid_json_drops\":%llu,\"decoder_truncation_drops\":%llu,\"queue_full_drops\":%llu,\"storage_ready\":%s,\"storage_drops\":%u,\"storage_free_bytes\":%llu,\"stored_sessions\":%u,\"recovered_sessions\":%u,\"storage_crc_failures\":%u,\"transport_drops\":%llu,\"receiver_drops\":%u,\"candidate_overflow\":%u}\n",
      request_id,capture_session.active()?"true":"false",capture_session.sessionId(),unsigned(capture_session.queued()),
      static_cast<unsigned long long>(counters.packet_records),static_cast<unsigned long long>(counters.pulse_records),
      radio_receiver::rawWindowActive()?"true":"false",radio_receiver::rawDropped(),radio_receiver::rawSkipped(),
      static_cast<unsigned long long>(counters.oversize_drops),
      static_cast<unsigned long long>(counters.invalid_json_drops),static_cast<unsigned long long>(counters.decoder_truncation_drops),static_cast<unsigned long long>(counters.queue_full_drops),
      storage.ready?"true":"false",storage.storage_drops,static_cast<unsigned long long>(storage.free_bytes),storage.sessions,
      storage.recovered_sessions,storage.crc_failures,static_cast<unsigned long long>(counters.transport_drops),
      radio_receiver::dropped(),capture_candidates.overflow());
  }
  else if(!strcmp(op,"capture_list")){
    Serial.printf("{\"event\":\"capture_list\",\"request_id\":%u,\"sessions\":[",request_id);
    bool first=true;
    for(const auto& info:wxconnect::capture::storage::sessions())if(info.occupied){
      Serial.printf("%s{\"session_id\":\"%s\",\"decoded_valid_bytes\":%u,\"decoded_records\":%u,\"pulse_valid_bytes\":%u,\"pulse_records\":%u,\"interrupted\":%s,\"corrupt\":%s,\"incomplete_tail\":%s}",
          first?"":",",info.id.data(),info.decoded_valid_bytes,info.decoded_records,
          info.pulse_valid_bytes,info.pulse_records,info.interrupted?"true":"false",
          info.corrupt?"true":"false",info.incomplete_tail?"true":"false");
      first=false;
    }
    Serial.println("]}");
  }
  else if(!strcmp(op,"capture_delete")){
    const char* session_id=doc["session_id"].is<const char*>()?doc["session_id"].as<const char*>():"";
    const bool deleted=wxconnect::capture::storage::deleteSession(session_id);
    Serial.printf("{\"event\":\"capture_delete\",\"request_id\":%u,\"ok\":%s}\n",
        request_id,deleted?"true":"false");
  }
  else if(!strcmp(op,"capture_read")){
    const char* session_id=doc["session_id"].is<const char*>()?doc["session_id"].as<const char*>():"";
    const char* stream=doc["stream"].is<const char*>()?doc["stream"].as<const char*>():"";
    const bool pulses=!strcmp(stream,"pulses");
    const bool valid_stream=pulses||!strcmp(stream,"decoded");
    size_t length=0;uint32_t next_offset=0;
    const uint32_t offset=doc["offset"]|uint32_t(0);
    const bool found=valid_stream&&wxconnect::capture::storage::readRecord(session_id,pulses,offset,
        capture_read_buffer,sizeof(capture_read_buffer),length,next_offset);
    if(found){Serial.write(reinterpret_cast<const uint8_t*>(capture_read_buffer),length);}
    Serial.printf("{\"event\":\"capture_read\",\"request_id\":%u,\"ok\":%s,\"stream\":\"%s\",\"offset\":%u,\"next_offset\":%u,\"done\":%s}\n",
        request_id,found?"true":"false",pulses?"pulses":"decoded",offset,next_offset,
        found&&next_offset<([&](){for(const auto& info:wxconnect::capture::storage::sessions())if(info.occupied&&!strcmp(info.id.data(),session_id))return pulses?info.pulse_valid_bytes:info.decoded_valid_bytes;return uint32_t(0);}())?"false":"true");
  }
  else if(!strcmp(op,"raw_window_start")){
    const uint32_t duration=doc["duration_ms"]|uint32_t(0);
    const bool started=capture_session.active()&&radio_receiver::startRawWindow(duration);
    Serial.printf("{\"event\":\"raw_window\",\"request_id\":%u,\"active\":%s,\"duration_ms\":%u,\"reason\":\"%s\"}\n",
        request_id,started?"true":"false",started?duration:0,
        !capture_session.active()?"capture_not_active":wxconnect::capture::validRawWindowDuration(duration)?"radio_unavailable":"duration_out_of_range");
  }
  else if(!strcmp(op,"raw_window_stop")){radio_receiver::stopRawWindow();drainPulseQueue();Serial.printf("{\"event\":\"raw_window\",\"request_id\":%u,\"active\":false}\n",request_id);}
  else if(!strcmp(op,"preview_select")){
    const char* protocol=doc["protocol"].is<const char*>()?doc["protocol"].as<const char*>():nullptr;
    const char* model=doc["model"].is<const char*>()?doc["model"].as<const char*>():nullptr;
    const char* id=doc["id"].is<const char*>()?doc["id"].as<const char*>():nullptr;
    const char* channel=doc["channel"].is<const char*>()?doc["channel"].as<const char*>():nullptr;
    bool found=false;
    for(const auto& candidate:capture_candidates.entries())if(candidate.occupied&&
        candidate.has_protocol==(protocol&&protocol[0])&&candidate.has_model==(model&&model[0])&&
        candidate.has_id==(id&&id[0])&&candidate.has_channel==(channel&&channel[0])&&
        (!candidate.has_protocol||!strcmp(candidate.protocol.data(),protocol))&&(!candidate.has_model||!strcmp(candidate.model.data(),model))&&
        (!candidate.has_id||!strcmp(candidate.id.data(),id))&&(!candidate.has_channel||!strcmp(candidate.channel.data(),channel))){
      found=selectPreview(candidate);break;
    }
    preview_explicit=found;dirty=true;
    Serial.printf("{\"event\":\"preview_select\",\"request_id\":%u,\"ok\":%s}\n",request_id,found?"true":"false");
  }
  else if(!strcmp(op,"preview_auto")){
    preview_explicit=false;refreshAutomaticPreview();dirty=true;
    Serial.printf("{\"event\":\"preview_auto\",\"request_id\":%u,\"ok\":true}\n",request_id);
  }
  else if(!strcmp(op,"rf_status")){if(rf_stream_until&&int32_t(rf_stream_until-millis())>0)rf_stream_until=millis()+15000;rfStatus();}
  else if(!strcmp(op,"rf_monitor")){
    const bool enabled=doc["enabled"]|false;rf_stream_until=enabled?millis()+15000:0;
    Serial.printf("{\"event\":\"rf_monitor\",\"request_id\":%u,\"enabled\":%s}\n",request_id,enabled?"true":"false");
  }
  else if(!strcmp(op,"theme_preview") && replay){int theme=doc["theme"]|-1;if(theme>=-1 && theme<=3){theme_preview=theme;dirty=true;status("theme_preview");}}
  else if(!strcmp(op,"diagnostics")){const auto r=radio_receiver::diagnostics();Serial.printf("{\"event\":\"diagnostics\",\"request_id\":%u,\"radio_ready\":%s,\"decoder_drops\":%u,\"portal_passed\":false}\n",request_id,r.ready?"true":"false",r.decoder_drops);}
  else if (!strcmp(op,"reset")) {resetData();status("reset");}
  else if (!strcmp(op,"page")) {
    if (doc["page"].is<unsigned>() && doc["page"].as<unsigned>()<3) {interact();page=doc["page"];measurement_detail=-1;dirty=true;status("page");}
  } else if(!strcmp(op,"detail")) {
    if(doc["detail"].is<unsigned>() && doc["detail"].as<unsigned>()<6){interact();measurement_detail=doc["detail"];dirty=true;status("detail");}
  } else if (!strcmp(op,"sample")) {
    if(!replay && store->accepted)resetData();
    if(have_sample_reply && request_id && request_id==last_sample_request_id){
      Serial.printf("{\"event\":\"sample\",\"request_id\":%u,\"result\":%u,\"accepted\":%u}\n",request_id,last_sample_result,store->accepted);
      request_id=0;return;
    }
    weather::Sample s;
    s.time_ms=doc["time_ms"]|uint64_t(0);s.station=doc["station"]|uint16_t(0);
    const char* channel=doc["channel"]|"";s.channel=channel[0];s.type=doc["type"]|uint8_t(0);
    s.exception=doc["exception"]|false;
    load(doc["temperature_F"],s.temperature);load(doc["humidity"],s.humidity);
    load(doc["wind_mph"],s.wind);load(doc["direction_deg"],s.direction);
    load(doc["lux"],s.lux);load(doc["uv"],s.uv);
    load(doc["rain_tips"],s.rain_tips);load(doc["strikes"],s.strikes);
    const uint32_t previous_rain=store->session_rain_tips;
    const auto previous_counter=store->rainState();
    auto result=store->ingest(s);
    sensor_monitor.observe(s,result);
    if(result==weather::Result::Duplicate){source_time=s.time_ms;last_receive=uptimeMs();replay=true;dirty=true;}
    if (result==weather::Result::Accepted) { source_time=s.time_ms;last_receive=uptimeMs();replay=true;dirty=true;
      if(s.rain_tips.valid)daily_rain.observe(localDay(s.time_ms),s.time_ms,store->session_rain_tips-previous_rain,previous_counter.baseline && s.time_ms>=previous_counter.time && s.time_ms-previous_counter.time<weather::kStaleMs && store->rain_discontinuities==previous_counter.discontinuities);
    }
    Serial.printf("{\"event\":\"sample\",\"request_id\":%u,\"result\":%u,\"accepted\":%u}\n",request_id,unsigned(result),store->accepted);
    last_sample_request_id=request_id;last_sample_result=unsigned(result);have_sample_reply=true;
  } else if (!strcmp(op,"screen") && display_ok) {
    if(setup_open){Serial.println("{\"event\":\"error\",\"reason\":\"setup_view_contains_key\"}");request_id=0;return;}
    draw();
    Serial.println("SCREEN RGB888 320 240 230400");
    uint8_t row[320*3];
    for(int y=0;y<240;++y){
      canvas.readRectRGB(0,y,320,1,row);
      // USB writes can be short when the host briefly stops draining data.
      // Retry the remaining bytes rather than silently dropping pixels.
      size_t sent=0;const uint64_t deadline=uptimeMs()+2000;
      while(sent<sizeof(row)){
        sent+=Serial.write(row+sent,sizeof(row)-sent);
        if(sent<sizeof(row)){if(uptimeMs()>=deadline){request_id=0;return;}delay(1);}
      }
    }
    Serial.println();
  } else {++serial_errors;Serial.println("{\"event\":\"error\",\"reason\":\"unknown_operation\"}");}
  request_id=0;
}
void receive() {
  // Bound serial work so a large replay cannot starve touch or rendering.
  for (unsigned n=0;n<256 && Serial.available();++n) {
    const int ch=Serial.read();
    if (ch=='\n') {
      if (!discard_line && line_size){line[line_size]=0;command(line);}
      else if(discard_line){++serial_errors;Serial.println("{\"event\":\"error\",\"reason\":\"line_too_long\"}");}
      line_size=0;discard_line=false;
    } else if(ch!='\r' && !discard_line) {
      if(line_size+1<sizeof(line))line[line_size++]=char(ch);else discard_line=true;
    }
  }
}
const wxconnect::capture::CandidateIdentity* soleCandidate(){
  const wxconnect::capture::CandidateIdentity* found=nullptr;
  for(const auto& candidate:capture_candidates.entries())if(candidate.occupied){
    if(found)return nullptr;
    found=&candidate;
  }
  return found;
}
bool previewMatches(const wxconnect::capture::CandidateIdentity& candidate){
  const auto& selected=preview_adapter.model().identity;
  return preview_adapter.model().selected&&candidate.has_protocol==selected.has_protocol&&
      candidate.has_model==selected.has_model&&candidate.has_id==selected.has_id&&
      candidate.has_channel==selected.has_channel&&
      (!candidate.has_protocol||candidate.protocol==selected.protocol)&&
      (!candidate.has_model||candidate.model==selected.model)&&
      (!candidate.has_id||candidate.id==selected.id)&&
      (!candidate.has_channel||candidate.channel==selected.channel);
}
bool selectPreview(const wxconnect::capture::CandidateIdentity& candidate){
  return preview_adapter.select(candidate.has_protocol?candidate.protocol.data():nullptr,
      candidate.has_model?candidate.model.data():nullptr,
      candidate.has_id?candidate.id.data():nullptr,
      candidate.has_channel?candidate.channel.data():nullptr);
}
void sendJson(JsonDocument& document,int status_code=200){
  String body;serializeJson(document,body);
  web_server.sendHeader("Cache-Control","no-store");
  web_server.sendHeader("X-Content-Type-Options","nosniff");
  web_server.send(status_code,"application/json",body);
}
void sendApiError(int status_code,const char* message){
  StaticJsonDocument<192> error;error["error"]=message;sendJson(error,status_code);
}
void rotateCsrf(){
  std::snprintf(csrf_token,sizeof(csrf_token),"%08lx%08lx%08lx%08lx",
      static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()),
      static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
}
bool authorizeWeb(bool mutation=false){
  const String admin=services::adminPassword();
  if(!web_server.authenticate("admin",admin.c_str())){
    web_server.requestAuthentication(BASIC_AUTH,"WXLink Connect");return false;
  }
  if(!mutation)return true;
  if(web_server.arg("plain").length()>1536){sendApiError(413,"request_too_large");return false;}
  const String token=web_server.header("X-CSRF-Token");
  if(!wxconnect::web_security::validCsrf(csrf_token,token.c_str())){sendApiError(403,"csrf_rejected");return false;}
  const String origin=web_server.header("Origin");
  if(origin.length()){
    const String host=web_server.hostHeader();
    if(!wxconnect::web_security::sameOrigin(origin.c_str(),host.c_str())){sendApiError(403,"origin_rejected");return false;}
  }
  return true;
}
void addIdentity(JsonObject object,const wxconnect::capture::CandidateIdentity& item){
  if(item.has_protocol)object["protocol"]=item.protocol.data();else object["protocol"]=nullptr;
  if(item.has_model)object["model"]=item.model.data();else object["model"]=nullptr;
  if(item.has_id)object["id"]=item.id.data();else object["id"]=nullptr;
  if(item.has_channel)object["channel"]=item.channel.data();else object["channel"]=nullptr;
  object["packets"]=item.packets;object["last_uptime_ms"]=item.last_uptime_ms;
  object["selected"]=previewMatches(item);
}
void addPreviewValue(JsonObject object,const char* name,const wxconnect::preview::Value& value){
  if(value.valid)object[name]=value.value;else object[name]=nullptr;
}
void connectStatusRoute(){
  if(!authorizeWeb())return;
  StaticJsonDocument<2048> document;
  const auto& model=preview_adapter.model();const auto& counters=capture_session.counters();
  const auto storage=wxconnect::capture::storage::state();
  document["csrf"]=csrf_token;document["event"]="connect_status";
  document["active"]=capture_session.active();document["session_id"]=capture_session.active()?capture_session.sessionId():nullptr;
  document["capture_owner"]=capture_lease.owner()==CaptureOwner::Browser?"browser":capture_lease.owner()==CaptureOwner::Usb?"usb":"none";
  document["capture_stop_reason"]=capture_stop_reason;
  document["session_age_ms"]=capture_session.active()?uptimeMs()-capture_started_at_ms:JsonVariant();
  const auto radio=radio_receiver::diagnostics();
  document["profile_id"]=wxconnect::radio_profile::kId;
  document["profile_revision"]=wxconnect::radio_profile::kRevisionNumber;
  document["requested_center_frequency_mhz"]=wxconnect::radio_profile::kCenterFrequencyMHz;
  document["requested_modulation"]=wxconnect::radio_profile::kModulation;
  document["requested_rx_bandwidth_khz"]=wxconnect::radio_profile::kRequestedRxBandwidthKHz;
  const auto profile_state=!radio.init_attempted
      ?wxconnect::radio_profile::InitializationState::NotInitialized
      :radio.ready?wxconnect::radio_profile::InitializationState::AcceptedUnverified
                  :wxconnect::radio_profile::InitializationState::Failed;
  document["radio_config_state"]=wxconnect::radio_profile::stateName(profile_state);
  document["radio_readback_verified"]=false;
  document["packet_records"]=counters.packet_records;
  document["pulse_records"]=counters.pulse_records;document["drops"]=counters.oversize_drops+
      counters.invalid_json_drops+counters.decoder_truncation_drops+counters.queue_full_drops+
      counters.storage_drops+counters.transport_drops;
  document["radio_ready"]=radio_receiver::ready();document["wifi"]=services::state().wifi;
  document["storage_ready"]=storage.ready;document["storage_free_bytes"]=storage.free_bytes;
  document["stored_sessions"]=storage.sessions;document["max_sessions"]=wxconnect::capture::storage::kSessionLimit;
  const auto transition=wxconnect::connect_migration::state();
  JsonObject main_transition=document.createNestedObject("main_transition");
  main_transition["worker_ready"]=transition.worker_ready;main_transition["busy"]=transition.busy;
  main_transition["restart_pending"]=transition.restart_pending;main_transition["progress"]=transition.progress;
  main_transition["phase"]=transition.phase==wxconnect::connect_migration::Phase::Checking?"checking":
      transition.phase==wxconnect::connect_migration::Phase::Downloading?"downloading":
      transition.phase==wxconnect::connect_migration::Phase::ReadyToRestart?"ready_to_restart":
      transition.phase==wxconnect::connect_migration::Phase::Failed?"failed":"idle";
  main_transition["target_version"]=transition.target_version[0]?transition.target_version:nullptr;
  main_transition["message"]=transition.message;
  JsonObject preview=document.createNestedObject("preview");
  preview["selected"]=model.selected;preview["adapter_available"]=model.adapter_available;
  if(model.selected){
    const auto& identity=model.identity;
    preview["protocol"]=identity.has_protocol?identity.protocol.data():nullptr;
    preview["model"]=identity.has_model?identity.model.data():nullptr;
    preview["id"]=identity.has_id?identity.id.data():nullptr;
    preview["channel"]=identity.has_channel?identity.channel.data():nullptr;
  }
  preview["packets"]=model.packets;
  preview["last_age_ms"]=model.last_packet_ms?uptimeMs()-model.last_packet_ms:JsonVariant();
  addPreviewValue(preview,"temperature_c",model.temperature_c);addPreviewValue(preview,"wind_mph",model.wind_mph);
  addPreviewValue(preview,"wind_direction_deg",model.wind_direction_deg);addPreviewValue(preview,"humidity_pct",model.humidity_pct);
  addPreviewValue(preview,"rain_counter",model.rain_counter);addPreviewValue(preview,"light_lux",model.light_lux);
  addPreviewValue(preview,"uv_index",model.uv_index);
  preview["rain_unit"]=model.rain_unit==wxconnect::preview::RainUnit::Inch?"in":
      model.rain_unit==wxconnect::preview::RainUnit::Millimetre?"mm":"";
  sendJson(document);
}
void devicesRoute(){
  if(!authorizeWeb())return;
  DynamicJsonDocument document(12288);document["csrf"]=csrf_token;
  JsonArray devices=document.createNestedArray("devices");
  for(const auto& item:capture_candidates.entries())if(item.occupied){JsonObject object=devices.createNestedObject();addIdentity(object,item);}
  if(document.overflowed()){sendApiError(503,"device_list_capacity_exceeded");return;}
  sendJson(document);
}
void configGetRoute(){
  if(!authorizeWeb())return;
  const auto& config=services::settings();StaticJsonDocument<1024> document;
  document["csrf"]=csrf_token;document["ssid"]=config.ssid;document["wifi_password_set"]=!config.wifi_password.isEmpty();
  document["timezone"]=config.timezone;document["display_theme"]=config.display_theme;
  document["date_format"]=config.date_format;document["clock_24h"]=config.clock_24h;
  document["display_sleep_mode"]=config.display_sleep_mode;document["display_timeout_s"]=config.display_timeout_s;
  document["admin_custom"]=config.admin_custom;sendJson(document);
}
bool allowedOption(const char* value,const char* const* options,size_t count){
  for(size_t i=0;i<count;++i)if(!std::strcmp(value,options[i]))return true;return false;
}
void configPostRoute(){
  if(!authorizeWeb(true))return;
  if(wxconnect::connect_migration::blocksCapture()){sendApiError(409,"firmware_transition_in_progress");return;}
  StaticJsonDocument<1024> document;
  if(deserializeJson(document,web_server.arg("plain"))||!document.is<JsonObject>()){
    sendApiError(400,"invalid_json");return;
  }
  const char* string_fields[]={"ssid","timezone","display_theme","date_format","display_sleep_mode","wifi_password","admin_password"};
  for(const char* field:string_fields)if(document.containsKey(field)&&!document[field].is<const char*>()){
    sendApiError(400,"invalid_field_type");return;
  }
  services::Settings next=services::settings();
  const char* ssid=document["ssid"]|next.ssid.c_str();
  const char* timezone=document["timezone"]|next.timezone.c_str();
  const char* theme=document["display_theme"]|next.display_theme.c_str();
  const char* date=document["date_format"]|next.date_format.c_str();
  const char* sleep=document["display_sleep_mode"]|next.display_sleep_mode.c_str();
  const char* password=document["wifi_password"]|"";
  const char* admin=document["admin_password"]|"";
  const char* themes[]={"midnight","matrix","amber","ice-blue","violet","paper","night"};
  const char* dates[]={"md","dm","iso","text"};const char* sleeps[]={"never","dim","off"};
  const char* zones[]={"EST5EDT,M3.2.0,M11.1.0","CST6CDT,M3.2.0,M11.1.0","MST7MDT,M3.2.0,M11.1.0","MST7","PST8PDT,M3.2.0,M11.1.0","UTC0"};
  if(std::strlen(ssid)>32||std::strlen(password)>63||std::strlen(admin)>63||
     !allowedOption(timezone,zones,sizeof(zones)/sizeof(zones[0]))||
     !allowedOption(theme,themes,sizeof(themes)/sizeof(themes[0]))||
     !allowedOption(date,dates,sizeof(dates)/sizeof(dates[0]))||
     !allowedOption(sleep,sleeps,sizeof(sleeps)/sizeof(sleeps[0]))){sendApiError(400,"setting_out_of_range");return;}
  if(admin[0]&&std::strlen(admin)<8){sendApiError(400,"admin_password_too_short");return;}
  if(document.containsKey("clock_24h")&&!document["clock_24h"].is<bool>()){
    sendApiError(400,"invalid_field_type");return;
  }
  if(document.containsKey("display_timeout_s")&&(!document["display_timeout_s"].is<uint16_t>()||
     document["display_timeout_s"].as<uint16_t>()<30||document["display_timeout_s"].as<uint16_t>()>3600)){
    sendApiError(400,"display_timeout_out_of_range");return;
  }
  next.ssid=ssid;next.timezone=timezone;next.display_theme=theme;next.date_format=date;next.display_sleep_mode=sleep;
  if(password[0])next.wifi_password=password;
  next.clock_24h=document["clock_24h"]|next.clock_24h;
  next.display_timeout_s=document["display_timeout_s"]|next.display_timeout_s;
  if(!services::saveConnectSettings(next)){sendApiError(503,"settings_storage_failed");return;}
  if(admin[0]&&!services::changeAdminPassword(admin)){sendApiError(503,"admin_password_storage_failed");return;}
  configTzTime(next.timezone.c_str(),"pool.ntp.org","time.nist.gov");rotateCsrf();
  StaticJsonDocument<128> response;response["ok"]=true;response["csrf"]=csrf_token;sendJson(response);
}
void captureStartRoute(){
  if(!authorizeWeb(true))return;
  if(wxconnect::connect_migration::blocksCapture()){sendApiError(409,"firmware_transition_in_progress");return;}
  if(capture_session.active()||!radio_receiver::ready()){sendApiError(409,"capture_busy_or_radio_unavailable");return;}
  if(services::clockValid())capture_clock.synchronize(millis(),uint64_t(time(nullptr))*1000);
  uint8_t entropy[16];for(size_t i=0;i<sizeof(entropy);i+=sizeof(uint32_t)){const uint32_t word=esp_random();memcpy(entropy+i,&word,sizeof(word));}
  char id[wxconnect::capture::kSessionIdChars+1];wxconnect::capture::formatSessionId(entropy,id);
  if(!wxconnect::capture::storage::beginSession(id)||!capture_session.start(id,wxconnect::capture::Source::Live,wxconnect::radio_profile::kId,millis(),capture_clock,capture_record_scratch)){
    if(wxconnect::capture::storage::state().active)wxconnect::capture::storage::finish();sendApiError(503,"capture_storage_unavailable");return;
  }
  capture_lease.begin(CaptureOwner::Browser,uptimeMs());
  capture_stop_reason="none";
  receiver_drops_at_start=radio_receiver::dropped();raw_drops_at_start=radio_receiver::rawDropped();raw_skips_at_start=radio_receiver::rawSkipped();
  capture_started_at_ms=uptimeMs();
  if(!writeCaptureRecord(capture_record_scratch)){sendApiError(507,"capture_storage_write_failed");return;}
  StaticJsonDocument<128> response;response["ok"]=true;response["session_id"]=id;sendJson(response,201);
}
void captureStopRoute(){
  if(!authorizeWeb(true))return;
  if(!capture_session.active()||capture_lease.owner()!=CaptureOwner::Browser){sendApiError(409,"browser_capture_not_active");return;}
  radio_receiver::stopRawWindow();liveReceive();drainCaptureQueue();
  if(!capture_session.stop(millis(),capture_clock,capture_record_scratch)){sendApiError(409,"capture_queue_pending");return;}
  attachCaptureDiagnostics("requested");
  const bool saved=writeCaptureRecord(capture_record_scratch)&&wxconnect::capture::storage::finish();capture_lease.clear();
  StaticJsonDocument<128> response;response["ok"]=true;response["storage_flushed"]=saved;sendJson(response,saved?200:507);
}
void captureHeartbeatRoute(){
  if(!authorizeWeb(true))return;
  StaticJsonDocument<128> request;
  if(deserializeJson(request,web_server.arg("plain"))||!request["session_id"].is<const char*>()){
    sendApiError(400,"invalid_json_or_session");return;
  }
  const char* id=request["session_id"];
  if(!capture_session.active()||capture_lease.owner()!=CaptureOwner::Browser||
     std::strcmp(id,capture_session.sessionId())){sendApiError(409,"browser_capture_changed");return;}
  capture_lease.heartbeat(CaptureOwner::Browser,uptimeMs());
  StaticJsonDocument<96> response;response["ok"]=true;response["session_id"]=id;sendJson(response);
}
void sessionsRoute(){
  if(!authorizeWeb())return;StaticJsonDocument<4096> document;document["csrf"]=csrf_token;
  JsonArray sessions=document.createNestedArray("sessions");
  for(const auto& info:wxconnect::capture::storage::sessions())if(info.occupied){JsonObject item=sessions.createNestedObject();
    item["session_id"]=info.id.data();item["decoded_records"]=info.decoded_records;item["pulse_records"]=info.pulse_records;
    item["decoded_valid_bytes"]=info.decoded_valid_bytes;item["pulse_valid_bytes"]=info.pulse_valid_bytes;
    item["interrupted"]=info.interrupted;item["corrupt"]=info.corrupt;item["incomplete_tail"]=info.incomplete_tail;}
  sendJson(document);
}
void exportRoute(){
  if(!authorizeWeb())return;
  const String id=web_server.arg("session"),stream=web_server.arg("stream"),offset_text=web_server.arg("offset"),revision_text=web_server.arg("revision");
  if(!wxconnect::web_security::validSessionId(id.c_str())||(stream!="decoded"&&stream!="pulses")||!offset_text.length()||!revision_text.length()){
    sendApiError(400,"invalid_export_request");return;
  }
  char* end=nullptr;const unsigned long parsed=strtoul(offset_text.c_str(),&end,10);
  if(!end||*end||parsed>UINT32_MAX){sendApiError(400,"invalid_offset");return;}
  end=nullptr;const unsigned long revision=strtoul(revision_text.c_str(),&end,10);
  if(!end||*end||revision>UINT32_MAX||parsed>=revision){sendApiError(400,"invalid_revision_or_offset");return;}
  const bool pulse_stream=stream=="pulses";size_t length=0;uint32_t next=0;
  if(!wxconnect::capture::storage::readRecord(id.c_str(),pulse_stream,uint32_t(parsed),capture_read_buffer,sizeof(capture_read_buffer)-1,length,next)){
    sendApiError(404,"record_unavailable");return;
  }
  capture_read_buffer[length]='\0';uint32_t valid=0;
  for(const auto& info:wxconnect::capture::storage::sessions())if(info.occupied&&id==info.id.data())valid=pulse_stream?info.pulse_valid_bytes:info.decoded_valid_bytes;
  if(revision>valid){sendApiError(409,"stream_revision_changed");return;}
  web_server.sendHeader("Cache-Control","no-store");web_server.sendHeader("X-Content-Type-Options","nosniff");
  web_server.sendHeader("X-Next-Offset",String(next));web_server.sendHeader("X-Stream-Revision",String(revision));
  web_server.sendHeader("X-Session-Done",next>=revision?"true":"false");
  web_server.send(200,"application/x-ndjson",capture_read_buffer);
}
void captureDeleteRoute(){
  if(!authorizeWeb(true))return;StaticJsonDocument<128> request;
  if(wxconnect::connect_migration::blocksCapture()){sendApiError(409,"firmware_transition_in_progress");return;}
  if(deserializeJson(request,web_server.arg("plain"))||!request["session_id"].is<const char*>()){
    sendApiError(400,"invalid_session_id");return;
  }
  const char* id=request["session_id"];
  if(std::strlen(id)!=wxconnect::capture::kSessionIdChars||
     !wxconnect::capture::storage::deleteSession(id)){sendApiError(409,"session_delete_failed_or_active");return;}
  StaticJsonDocument<64> response;response["ok"]=true;sendJson(response);
}
void previewRoute(){
  if(!authorizeWeb(true))return;StaticJsonDocument<512> request;
  if(deserializeJson(request,web_server.arg("plain"))||!request.is<JsonObject>()){
    sendApiError(400,"invalid_device_identity");return;
  }
  const auto textValue=[&](const char* key)->const char*{return request[key].is<const char*>()?request[key].as<const char*>():nullptr;};
  const char* protocol=textValue("protocol"),*model=textValue("model"),*id=textValue("id"),*channel=textValue("channel");
  for(const auto& candidate:capture_candidates.entries())if(candidate.occupied&&
      candidate.has_protocol==bool(protocol&&protocol[0])&&candidate.has_model==bool(model&&model[0])&&
      candidate.has_id==bool(id&&id[0])&&candidate.has_channel==bool(channel&&channel[0])&&
      (!candidate.has_protocol||!std::strcmp(candidate.protocol.data(),protocol))&&
      (!candidate.has_model||!std::strcmp(candidate.model.data(),model))&&
      (!candidate.has_id||!std::strcmp(candidate.id.data(),id))&&
      (!candidate.has_channel||!std::strcmp(candidate.channel.data(),channel))){
    if(!selectPreview(candidate)){sendApiError(400,"preview_selection_rejected");return;}
    preview_explicit=true;StaticJsonDocument<64> response;response["ok"]=true;sendJson(response);return;
  }
  sendApiError(404,"device_not_found");
}
void rawWindowRoute(){
  if(!authorizeWeb(true))return;StaticJsonDocument<128> request;
  if(deserializeJson(request,web_server.arg("plain"))){sendApiError(400,"invalid_json");return;}
  const uint32_t duration=request["duration_ms"]|uint32_t(0);
  const bool started=capture_session.active()&&radio_receiver::startRawWindow(duration);
  if(!started){sendApiError(400,"raw_window_not_started");return;}
  StaticJsonDocument<64> response;response["ok"]=true;response["duration_ms"]=duration;sendJson(response,202);
}
void switchToMainRoute(){
  if(!authorizeWeb(true))return;
  StaticJsonDocument<128> request;
  if(deserializeJson(request,web_server.arg("plain"))||!request.is<JsonObject>()||
     !request["confirmation"].is<const char*>()||
     std::strcmp(request["confirmation"].as<const char*>(),"SWITCH TO WX-LINK")){
    sendApiError(400,"explicit_switch_confirmation_required");return;
  }
  if(capture_session.active()||capture_lease.owner()!=CaptureOwner::None||
     wxconnect::capture::storage::state().active||radio_receiver::rawWindowActive()){
    sendApiError(409,"stop_and_finish_capture_before_switching");return;
  }
  if(WiFi.status()!=WL_CONNECTED||!services::clockValid()){
    sendApiError(409,"home_wifi_and_valid_time_required");return;
  }
  if(!wxconnect::connect_migration::request()){
    sendApiError(409,"firmware_transition_unavailable_or_busy");return;
  }
  StaticJsonDocument<128> response;response["ok"]=true;
  response["message"]="WX-Link release check queued";sendJson(response,202);
}
void beginWebServer(){
  rotateCsrf();const char* headers[]={"Origin","X-CSRF-Token"};web_server.collectHeaders(headers,2);
  web_server.on("/",HTTP_GET,[]{if(!authorizeWeb())return;web_server.sendHeader("Cache-Control","no-store");web_server.send_P(200,"text/html; charset=utf-8",kSetupPage);});
  web_server.on("/api/config",HTTP_GET,configGetRoute);web_server.on("/api/config",HTTP_POST,configPostRoute);
  web_server.on("/api/connect/status",HTTP_GET,connectStatusRoute);web_server.on("/api/connect/devices",HTTP_GET,devicesRoute);
  web_server.on("/api/connect/sessions",HTTP_GET,sessionsRoute);web_server.on("/api/connect/export",HTTP_GET,exportRoute);
  web_server.on("/api/connect/start",HTTP_POST,captureStartRoute);web_server.on("/api/connect/stop",HTTP_POST,captureStopRoute);
  web_server.on("/api/connect/heartbeat",HTTP_POST,captureHeartbeatRoute);
  web_server.on("/api/connect/raw-window",HTTP_POST,rawWindowRoute);web_server.on("/api/connect/preview",HTTP_POST,previewRoute);
  web_server.on("/api/connect/delete",HTTP_POST,captureDeleteRoute);
  web_server.on("/api/connect/switch-to-wx-link",HTTP_POST,switchToMainRoute);
  web_server.onNotFound([]{web_server.sendHeader("Cache-Control","no-store");web_server.send(404,"application/json","{\"error\":\"not_found\"}");});
  web_server.begin();
}
void refreshAutomaticPreview(){
  if(preview_explicit)return;
  const auto* candidate=soleCandidate();
  if(!candidate){preview_adapter.clearSelection();return;}
  if(!previewMatches(*candidate))selectPreview(*candidate);
}
void touch() {
  static bool down=false;
  int16_t x=0,y=0;
  const bool pressed=display_ok && display.getTouch(&x,&y);
  if(pressed&&!down){++touch_events;interact();down=true;}
  else if(!pressed&&down){down=false;interact();setup_open=!setup_open;dirty=true;}
}
void liveReceive(){
  char (&packet)[radio_receiver::kDecodedBufferBytes]=radio_packet_copy;
  for(int count=0;count<4;++count){
    size_t received_length=0;bool truncated=false;
    if(!radio_receiver::pop(packet,received_length,truncated))break;
    if(replay)continue;
    const size_t native_length=received_length;
    if(capture_session.active()){
      if(services::clockValid())capture_clock.synchronize(millis(),uint64_t(time(nullptr))*1000);
      capture_session.enqueueDecoderOutput(packet,native_length,truncated,millis(),capture_clock);
    }
    if(truncated)continue;
    StaticJsonDocument<2048>d;if(deserializeJson(d,packet))continue;
    char protocol[128],model[128],identity[128],channel[128];
    const bool has_protocol=nativeField(d.as<JsonVariantConst>(),"protocol",protocol,sizeof(protocol));
    const bool has_model=nativeField(d.as<JsonVariantConst>(),"model",model,sizeof(model));
    const bool has_id=nativeField(d.as<JsonVariantConst>(),"id",identity,sizeof(identity));
    const bool has_channel=nativeField(d.as<JsonVariantConst>(),"channel",channel,sizeof(channel));
    capture_candidates.observe(has_protocol?protocol:nullptr,has_model?model:nullptr,
                               has_id?identity:nullptr,has_channel?channel:nullptr,uptimeMs());
    refreshAutomaticPreview();
    preview_adapter.ingest(packet,native_length,uptimeMs());
    station_discovery::Packet candidate;
    if(station_discovery::decodeNative(d.as<JsonVariantConst>(),candidate))services::observeDiscovery(candidate,millis());
    if(!services::settings().station_confirmed)continue;
    weather::Sample s;if(!atlas::decode(d.as<JsonVariantConst>(),liveNow(),s))continue;
    uint32_t previous=store->session_rain_tips;auto counter=store->rainState();
    const auto result=store->ingest(s);sensor_monitor.observe(s,result);
    if(Serial&&rf_stream_until&&int32_t(rf_stream_until-millis())>0){
      StaticJsonDocument<2048> record;record["event"]="rf_packet";record["uptime_ms"]=millis();
      record["result"]=unsigned(result);record["sensor_status"]=sensor_health::label(sensorState());record["packet"]=d.as<JsonObjectConst>();
      char encoded[1536];const size_t length=serializeJson(record,encoded,sizeof(encoded));
      Serial.write(reinterpret_cast<const uint8_t*>(encoded),length);Serial.println();
    }
    if(result==weather::Result::Accepted){
      if(s.rain_tips.valid && clock_anchored)daily_rain.observe(localDay(s.time_ms),s.time_ms,store->session_rain_tips-previous,counter.baseline && s.time_ms>=counter.time && s.time_ms-counter.time<weather::kStaleMs && counter.discontinuities==store->rain_discontinuities);
      dirty=true;
    }
  }
  drainCaptureQueue();
  drainPulseQueue();
}
void publishWeather(){
  services::Observation o;uint64_t now=dataNow();o.epoch_ms=store->latest_ms;o.live=!replay && clock_anchored;o.fresh=sensor_monitor.complete(now,*store)&&sensorState()==sensor_health::State::Current;
  o.sensor_health=uint8_t(sensorState());if(sensor_monitor.received&&now>=sensor_monitor.last_rx)o.sensor_packet_age_s=int32_t((now-sensor_monitor.last_rx)/1000);
  o.temperature_valid=store->temperature.fresh(now);o.temperature_f=store->temperature.value;
  o.humidity_valid=store->humidity.fresh(now);o.humidity=store->humidity.value;
  o.wind_valid=store->wind.fresh(now);o.wind_mph=store->wind.value;o.direction_valid=store->direction.fresh(now);o.direction_deg=store->direction.value;
  auto gust=store->gust(now);o.gust_valid=gust.valid && store->gustComplete(now);o.gust_mph=gust.value;
  o.gust_observed=gust.valid;o.gust_partial=gust.valid&&!store->gustComplete(now);
  auto displayed=[&](const auto& field,uint8_t bit){if(field.valid)o.display_present|=bit;if(field.valid&&!field.fresh(now,sensor_health::kDataStaleMs))o.display_stale|=bit;};
  displayed(store->temperature,services::Temperature);displayed(store->humidity,services::Humidity);
  displayed(store->wind,services::Wind);displayed(store->direction,services::Direction);
  displayed(store->lux,services::Light);displayed(store->uv,services::UV);
  o.lux_valid=store->lux.fresh(now);o.lux=store->lux.value;o.uv_valid=store->uv.fresh(now);o.uv=store->uv.value;
  const auto rain_today=todayRain(now);o.rain_today_observed=rain_today.valid;
  o.display_rain_partial=!rain_today.complete;
  const auto counter=store->rainState();
  auto rain24=store->rain_timeline.window(now,weather::kRainDayMs);
  auto rain_hour=store->rain_timeline.window(now,3600000);
  o.rain_24h_observed=rain24.valid;o.rain_24h_valid=rain24.valid&&rain24.complete;o.rain_24h_in=rain24.tips/100.0f;
  o.rain_hour_valid=rain_hour.valid&&rain_hour.complete;o.rain_hour_in=rain_hour.tips/100.0f;
  o.rain_today_valid=rain_today.valid&&rain_today.complete&&now>=counter.time&&now-counter.time<weather::kStaleMs;o.rain_today_in=rain_today.tips/100.0f;
  o.radio_ready=radio_receiver::ready();o.packets=store->accepted;o.dropped=radio_receiver::dropped();services::publish(o);
}
}  // namespace

void setup() {
  // A stalled new image must reset before it can be confirmed healthy.
  esp_task_wdt_init(30,true);enableLoopWDT();
  Serial.setRxBufferSize(2048);Serial.setTxBufferSize(4096);
  Serial.begin(115200);Serial.setTxTimeoutMs(20);
  setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1);tzset();
  history=static_cast<weather::Bucket*>(heap_caps_malloc(sizeof(weather::Bucket)*weather::kHistoryBuckets,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if (!history) {Serial.println("{\"event\":\"fatal\",\"reason\":\"history_allocation_failed\"}");return;}
  for(size_t i=0;i<weather::kHistoryBuckets;++i)new(history+i)weather::Bucket{};
  rain_history=static_cast<weather::RainInterval*>(heap_caps_calloc(weather::kRainIntervals,sizeof(weather::RainInterval),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!rain_history){Serial.println("{\"event\":\"fatal\",\"reason\":\"rain_history_allocation_failed\"}");return;}
  store=new weather::Store(history,679,'A',rain_history);
  services::bindHistory(store,dataNow);
  display_ok=display.init();display.setRotation(1);display.setBrightness(0);display_brightness=0;
  canvas.setPsram(true);canvas.setColorDepth(16);
  display_ok=display_ok && canvas.createSprite(320,240)!=nullptr;
  canvas.setTextDatum(textdatum_t::top_left);
  if(display_ok)draw();
  services::begin();
  wxconnect::connect_migration::begin();
  beginWebServer();
  wxconnect::capture::storage::begin();
  store->~Store();new(store)weather::Store(history,services::settings().station_id,services::settings().channel,rain_history);
  if(services::settings().radio_enabled)radio_receiver::begin();
  display_idle.interact(uptimeMs());
  if(display_ok){draw();display_brightness=normalBrightness();display.setBrightness(display_brightness);}
  status("boot");
}
void loop() {
  if(!store){delay(100);return;}
  receive();touch();services::loop();web_server.handleClient();radio_receiver::loop();
  if(wxconnect::connect_migration::restartDue())esp_restart();
  liveReceive();
  if(capture_session.active()&&capture_lease.expired(uptimeMs())){
    radio_receiver::stopRawWindow();
    liveReceive();
    if(capture_session.stop(millis(),capture_clock,capture_record_scratch)){
      attachCaptureDiagnostics("lease_expired");
      const bool saved=writeCaptureRecord(capture_record_scratch);
      if(saved)wxconnect::capture::storage::finish();
      capture_lease.clear();
      if(saved)capture_stop_reason="lease_expired";
      Serial.println("{\"event\":\"capture_lease_expired\",\"stopped\":true}");
    }
  }
  if(capture_session.active()&&!wxconnect::capture::storage::tick()){
    capture_session.storageDrop();capture_stop_reason="storage_flush_failed";
    capture_session.abortForStorageFailure();wxconnect::capture::storage::finish();
    capture_lease.clear();
  }
  const uint64_t now=uptimeMs();
  if(display_idle.returnHome(now,(page==1||page==2)&&measurement_detail<0&&!setup_open&&!alerts_open,false)){
    page=0;detail_scroll=0;dirty=true;
  }
  const auto& preferences=services::settings();
  const uint8_t brightness=display_idle.brightness(now,preferences.display_sleep_mode.c_str(),preferences.display_timeout_s,normalBrightness());
  if(display_ok&&brightness!=display_brightness){display_brightness=brightness;display.setBrightness(brightness);}
  if(services::clockValid() && !clock_anchored){clock_epoch=uint64_t(time(nullptr))*1000;clock_uptime=now;clock_anchored=true;if(!replay)resetData();}
  if(now-last_publish>=1000){publishWeather();last_publish=now;}
  if((dirty && now-last_draw>=100) || now-last_draw>=1000)draw();
  if(now-last_status>=10000){status();last_status=now;}
  delay(2);
}
