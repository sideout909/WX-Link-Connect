#include "checkpoint.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <memory>
#include <new>
namespace checkpoint {
namespace {
constexpr size_t kBucketsBytes=sizeof(weather::Bucket)*weather::kHistoryBuckets;
constexpr size_t kRainBytes=sizeof(weather::RainInterval)*weather::kRainIntervals;
struct Header {
  uint32_t magic=0x57584c33,version=3,bytes=kBucketsBytes+kRainBytes,crc=0;
  uint64_t saved=0;uint16_t station=0;char channel='A';uint8_t reserved=0;
  weather::Store::RainState counter;weather::DailyRain daily;
  weather::RollingRain::State timeline;
};
bool mounted=false;
uint32_t crc32(const uint8_t* p,size_t n,uint32_t crc=0xffffffff){while(n--){crc^=*p++;for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}return crc;}
uint32_t checksum(Header h,const weather::Bucket* b,const weather::RainInterval* rain){h.crc=0;return ~crc32(reinterpret_cast<const uint8_t*>(rain),kRainBytes,crc32(reinterpret_cast<const uint8_t*>(b),kBucketsBytes,crc32(reinterpret_cast<const uint8_t*>(&h),sizeof(h))));}
using Memory=std::unique_ptr<uint8_t,decltype(&heap_caps_free)>;
Memory allocate(size_t bytes){return Memory(static_cast<uint8_t*>(heap_caps_calloc(1,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)),heap_caps_free);}
}
bool begin(){mounted=LittleFS.begin(false);if(mounted&&!LittleFS.exists("/connect"))mounted=LittleFS.mkdir("/connect");return mounted;}
bool restore(weather::Bucket* buckets,weather::Store& store,weather::DailyRain& rain,uint64_t now,uint16_t station,char channel,const char* path){
  if(!mounted||!store.rain_timeline.data())return false;File file=LittleFS.open(path,"r");Header h;
  if(!file||file.size()!=sizeof(h)+h.bytes||file.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))!=sizeof(h))return false;
  if(h.magic!=0x57584c33||h.version!=3||h.bytes!=kBucketsBytes+kRainBytes||h.station!=station||h.channel!=channel||h.saved>now||now-h.saved>86400000||h.timeline.head>=weather::kRainIntervals||h.timeline.count>weather::kRainIntervals)return false;
  auto staged=allocate(h.bytes);if(!staged)return false;auto* staged_buckets=reinterpret_cast<weather::Bucket*>(staged.get());auto* staged_rain=reinterpret_cast<weather::RainInterval*>(staged.get()+kBucketsBytes);
  bool valid=file.read(staged.get(),h.bytes)==h.bytes&&checksum(h,staged_buckets,staged_rain)==h.crc;
  if(valid){memcpy(buckets,staged_buckets,kBucketsBytes);memcpy(store.rain_timeline.data(),staged_rain,kRainBytes);store.rain_timeline.restore(h.timeline);store.restoreRain(h.counter);rain=h.daily;rain.complete=false;}
  return valid;
}
bool save(const weather::Bucket* buckets,const weather::Store& store,const weather::DailyRain& rain,uint64_t now,uint16_t station,char channel,const char* path){
  if(!mounted||!store.rain_timeline.data())return false;Header h;memset(&h,0,sizeof(h));h.magic=0x57584c33;h.version=3;h.bytes=kBucketsBytes+kRainBytes;
  h.saved=now;h.station=station;h.channel=channel;h.counter=store.rainState();h.daily=rain;h.timeline=store.rain_timeline.state();h.crc=checksum(h,buckets,store.rain_timeline.data());
  String temp=String(path)+".tmp";File f=LittleFS.open(temp,"w");if(!f)return false;
  bool ok=f.write(reinterpret_cast<const uint8_t*>(&h),sizeof(h))==sizeof(h)&&f.write(reinterpret_cast<const uint8_t*>(buckets),kBucketsBytes)==kBucketsBytes&&f.write(reinterpret_cast<const uint8_t*>(store.rain_timeline.data()),kRainBytes)==kRainBytes;
  f.flush();f.close();if(ok)ok=LittleFS.rename(temp,path);return ok;
}
bool selftest(){
  if(!mounted)return false;auto first=allocate(kBucketsBytes+kRainBytes),second=allocate(kBucketsBytes+kRainBytes);if(!first||!second)return false;
  auto* b1=reinterpret_cast<weather::Bucket*>(first.get());auto* b2=reinterpret_cast<weather::Bucket*>(second.get());
  for(size_t i=0;i<weather::kHistoryBuckets;++i){new(b1+i)weather::Bucket{};new(b2+i)weather::Bucket{};}
  auto* r1=reinterpret_cast<weather::RainInterval*>(first.get()+kBucketsBytes);auto* r2=reinterpret_cast<weather::RainInterval*>(second.get()+kBucketsBytes);
  std::unique_ptr<weather::Store> model(new(std::nothrow)weather::Store(b1,679,'A',r1)),restored(new(std::nothrow)weather::Store(b2,679,'A',r2));if(!model||!restored)return false;
  constexpr uint64_t epoch=1792281600000ULL;weather::Sample s;s.time_ms=epoch;s.type=38;s.rain_tips.valid=true;s.rain_tips.value=25;s.temperature.valid=true;s.temperature.value=55.25f;model->ingest(s);
  s.time_ms+=30000;s.rain_tips.value=26;model->ingest(s);weather::DailyRain rain;rain.day=20261018;rain.valid=true;rain.complete=true;rain.tips=1;rain.last_time=s.time_ms;weather::DailyRain recovered;
  const char* path="/connect/checkpoint-selftest.bin";bool ok=save(b1,*model,rain,s.time_ms,679,'A',path)&&save(b1,*model,rain,s.time_ms,679,'A',path);
  ok=ok&&!restore(b2,*restored,recovered,s.time_ms,680,'A',path)&&!restore(b2,*restored,recovered,s.time_ms-1,679,'A',path)&&restore(b2,*restored,recovered,s.time_ms+60000,679,'A',path);
  const auto* b=restored->bucket(epoch/60000);auto total=restored->rain_timeline.window(s.time_ms,weather::kRainDayMs);
  ok=ok&&b&&b->temperature_count==2&&b->temperature_sum==110.5f&&restored->rainState().counter==26&&recovered.tips==1&&!recovered.complete&&total.valid&&total.tips==1&&!total.complete;
  ok=ok&&restored->accepted==0&&!restored->temperature.valid&&restored->historySummary(s.time_ms+60000).buckets==1;
  File corrupt=LittleFS.open(path,"r+");if(corrupt){corrupt.seek(sizeof(Header)+kBucketsBytes+4);int byte=corrupt.read();corrupt.seek(sizeof(Header)+kBucketsBytes+4);corrupt.write(uint8_t(byte^1));corrupt.close();}else ok=false;
  ok=ok&&!restore(b2,*restored,recovered,s.time_ms+60000,679,'A',path);LittleFS.remove(path);LittleFS.remove(String(path)+".tmp");return ok;
}
}
