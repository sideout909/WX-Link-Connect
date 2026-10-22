#include "archive_storage.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <memory>
#include <time.h>

namespace archive_storage {
namespace {
using namespace history_archive;
constexpr const char* kPath="/connect/archive-v1.bin";
constexpr size_t kFileBytes=sizeof(Record)*kRecords,kReserveBytes=512*1024;
File file;
uint32_t* intervals=nullptr;
std::unique_ptr<Accumulator> accumulator;
uint16_t station_id=0;char station_channel='A';
bool ready=false,resumed=false;
uint32_t failures=0,invalid=0;
uint64_t last_saved=0;
String message="Archive not initialized";
bool readRecord(File& f,uint32_t interval,Record& r){
  return f.seek((interval%kRecords)*sizeof(Record))&&f.read(reinterpret_cast<uint8_t*>(&r),sizeof(r))==sizeof(r)&&r.interval==interval&&valid(r,station_id,station_channel);
}
bool writeRecord(File& f,const Record& r){
  if(!f.seek((r.interval%kRecords)*sizeof(Record)))return false;
  const bool ok=f.write(reinterpret_cast<const uint8_t*>(&r),sizeof(r))==sizeof(r);f.flush();return ok;
}
void persist(const std::optional<Record>& r){
  if(!ready||!r)return;
  if(LittleFS.totalBytes()-LittleFS.usedBytes()<kReserveBytes){++failures;message="Archive paused: storage reserve reached";return;}
  if(writeRecord(file,*r)){intervals[r->interval%kRecords]=r->interval;last_saved=uint64_t(r->interval)*kIntervalMs;message="Recording five-minute summaries";}
  else{++failures;message="Archive write failed";}
}
void resume(uint64_t now){
  if(resumed)return;resumed=true;
  Record r;const uint32_t interval=now/kIntervalMs;
  if(readRecord(file,interval,r))accumulator->resume(r);
}
String value(float v,bool present,unsigned decimals=2){return present?String(v,decimals):String("");}
}
bool begin(uint16_t station,char channel){
  station_id=station;station_channel=channel;
  intervals=static_cast<uint32_t*>(heap_caps_calloc(kRecords,sizeof(uint32_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  accumulator.reset(new(std::nothrow)Accumulator(station,channel));
  if(!intervals||!accumulator){message="Archive memory allocation failed";return false;}
  if(!LittleFS.totalBytes()){message="Archive filesystem unavailable";return false;}
  if(!LittleFS.exists("/connect")&&!LittleFS.mkdir("/connect")){message="Connect storage directory unavailable";return false;}
  if(!LittleFS.exists(kPath)){
    if(LittleFS.totalBytes()-LittleFS.usedBytes()<kFileBytes+kReserveBytes){message="Not enough archive storage headroom";return false;}
    File initial=LittleFS.open("/connect/archive-v1.tmp","w");uint8_t zero[1024]{};bool ok=bool(initial);
    for(size_t at=0;ok&&at<kFileBytes;at+=sizeof(zero)){const size_t n=std::min(sizeof(zero),kFileBytes-at);ok=initial.write(zero,n)==n;}
    initial.flush();initial.close();
    if(!ok||!LittleFS.rename("/connect/archive-v1.tmp",kPath)){message="Archive allocation failed";return false;}
  }
  file=LittleFS.open(kPath,"r+");
  if(!file||file.size()!=kFileBytes){message="Archive format/size mismatch";return false;}
  Record r;
  for(uint32_t i=0;i<kRecords;++i){
    if(file.read(reinterpret_cast<uint8_t*>(&r),sizeof(r))!=sizeof(r)){message="Archive scan failed";return false;}
    if(!r.interval)continue;
    if(valid(r,station,channel)&&r.interval%kRecords==i){intervals[i]=r.interval;last_saved=std::max(last_saved,uint64_t(r.interval)*kIntervalMs);}
    else if(r.station==station&&r.channel==uint8_t(channel))++invalid;
  }
  ready=true;message="Recording five-minute summaries";return true;
}
void loop(uint64_t now,bool current){
  if(!ready||now<1700000000000ULL)return;
  resume(now);persist(accumulator->tick(now,current));
}
void observe(const weather::Sample& s,weather::Result result,uint32_t delta,uint64_t previous,bool continuous){
  if(!ready||s.time_ms<1700000000000ULL)return;
  resume(s.time_ms);persist(accumulator->observe(s,result,delta,previous,continuous));
}
bool flush(){
  if(!ready||!accumulator->active())return false;
  const auto before=failures;persist(accumulator->snapshot());return failures==before;
}
uint32_t endInterval(uint64_t now){
  const uint32_t current=now/kIntervalMs;
  return current&&ready&&intervals[current%kRecords]!=current?current-1:current;
}
State state(uint64_t now){
  State s;s.ready=ready;s.message=message;s.bytes=ready?kFileBytes:0;s.free_bytes=LittleFS.totalBytes()-LittleFS.usedBytes();
  s.write_failures=failures;s.invalid_records=invalid;s.last_saved_ms=last_saved;
  if(ready&&now>=1700000000000ULL){const uint32_t end=endInterval(now);for(uint32_t i=0;i<kRecords;++i)if(intervals[i]&&inWindow(intervals[i],end)){
    const uint64_t start=uint64_t(intervals[i])*kIntervalMs;++s.records;
    if(!s.oldest_ms||start<s.oldest_ms)s.oldest_ms=start;s.latest_ms=std::max(s.latest_ms,start);
  }}return s;
}
const char* csvHeader(){return "utc_start,station_id,channel,interval_seconds,temperature_mean_F,temperature_min_F,temperature_max_F,humidity_mean_pct,lux_mean,lux_peak,wind_mean_mph,wind_peak_mph,rain_observed_in,rain_complete,temperature_samples,humidity_samples,lux_samples,wind_samples,accepted,repeats,rejected,current_seconds,observed_seconds,rain_samples,partial,saturated\n";}
String csvPage(uint32_t end,uint16_t offset){
  String csv;if(!ready||offset>=kRecords)return csv;csv.reserve(8192);
  const uint32_t first=end>=kRecords-1?end-(kRecords-1):1;
  for(uint32_t at=offset;at<std::min(uint32_t(kRecords),uint32_t(offset+kPageSize));++at){
    const uint32_t interval=first+at;if(interval>end)break;
    Record r;if(!inWindow(interval,endInterval(uint64_t(time(nullptr))*1000))||!readRecord(file,interval,r))continue;
    time_t stamp=uint64_t(interval)*300;struct tm t{};gmtime_r(&stamp,&t);char iso[24];strftime(iso,sizeof(iso),"%Y-%m-%dT%H:%M:%SZ",&t);
    csv+=String(iso)+","+String(r.station)+","+char(r.channel)+",300,";
    csv+=value(r.temperature_mean,r.temperature_count)+","+value(r.temperature_min,r.temperature_count)+","+value(r.temperature_max,r.temperature_count)+",";
    csv+=value(r.humidity_tenths/10.0f,r.humidity_count,1)+","+(r.lux_count?String(r.lux_mean):String(""))+","+(r.lux_count?String(r.lux_peak):String(""))+",";
    csv+=value(r.wind_mean,r.wind_count)+","+value(r.wind_peak,r.wind_count)+","+value(r.rain_tips/100.0f,r.rain_samples)+","+String(r.rain_samples&&!(r.flags&kRainPartial)?1:0)+",";
    csv+=String(r.temperature_count)+","+String(r.humidity_count)+","+String(r.lux_count)+","+String(r.wind_count)+","+String(r.accepted)+","+String(r.repeats)+","+String(r.rejected)+",";
    csv+=String(r.current_seconds)+","+String(r.observed_seconds)+","+String(r.rain_samples)+","+String(bool(r.flags&kPartial))+","+String(bool(r.flags&kSaturated))+"\n";
  }return csv;
}
bool selftest(){
  const char* path="/connect/archive-selftest.bin";File f=LittleFS.open(path,"w+");if(!f)return false;
  Record r{};r.interval=kRecords*1000;r.station=station_id;r.channel=station_channel;r.observed_seconds=5;r.flags=kPartial|kRainPartial;r.crc=checksum(r);
  bool ok=writeRecord(f,r);Record recovered{};ok=ok&&readRecord(f,r.interval,recovered);
  r.interval+=kRecords;r.crc=checksum(r);ok=ok&&writeRecord(f,r)&&!readRecord(f,r.interval-kRecords,recovered);f.close();
  f=LittleFS.open(path,"r+");ok=ok&&readRecord(f,r.interval,recovered);
  if(f.seek(12)){const int b=f.read();ok=ok&&f.seek(12)&&f.write(uint8_t(b^1))==1;f.flush();}else ok=false;
  ok=ok&&!readRecord(f,r.interval,recovered);f.close();LittleFS.remove(path);return ok;
}
}
