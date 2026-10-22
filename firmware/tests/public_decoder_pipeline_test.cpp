#include "capture_session.h"
#include "connect_preview.h"
#include "connect_live.h"
#include <fstream>
#include <string>
#include <cassert>
#include <iostream>
using namespace wxconnect;
static std::string identity(JsonVariantConst value) {
  if(value.isNull())return {};
  if(value.is<const char*>())return value.as<const char*>();
  std::string result;serializeJson(value,result);return result;
}
int main(int argc,char** argv){
  assert(argc==2);std::ifstream file(argv[1]);assert(file);
  capture::CaptureSession recording;capture::ClockMap clock;capture::Record record;
  assert(recording.start("0123456789abcdef0123456789abcdef",capture::Source::Replay,"public-replay",0,clock,record));
  preview::Adapter adapter;capture::CandidateTable candidates;LivePacketView live;
  unsigned count=0;uint64_t now=0;std::string line;
  while(std::getline(file,line)){
    if(line.empty())continue;
    for(unsigned repeat=0;repeat<2;++repeat){
      now+=1000;++count;
      assert(recording.enqueueDecoderOutput(line.c_str(),line.size(),false,now,clock)==capture::PushResult::Queued);
      assert(live.observe(line.c_str(),line.size(),false,now));
      StaticJsonDocument<2048> document;
      assert(preview::parseNative(line.c_str(),line.size(),document));
      const auto protocol=identity(document["protocol"]), model=identity(document["model"]);
      const auto id=identity(document["id"]),channel=identity(document["channel"]);
      assert(candidates.observe(protocol.empty()?nullptr:protocol.c_str(),model.c_str(),
          id.empty()?nullptr:id.c_str(),channel.empty()?nullptr:channel.c_str(),now));
      if(!adapter.model().selected)assert(adapter.select(protocol.empty()?nullptr:protocol.c_str(),model.c_str(),
          id.empty()?nullptr:id.c_str(),channel.empty()?nullptr:channel.c_str()));
      assert(adapter.model().adapter_available);
      const bool accepted=adapter.ingest(line.c_str(),line.size(),now);
      if(accepted){
        assert(adapter.model().last_packet_ms==now);
        const auto& m=adapter.model();
        if(!document["temperature_C"].isNull()){
          assert(m.temperature_c.valid);assert(std::fabs(m.temperature_c.value-document["temperature_C"].as<double>())<0.001);
        }
        if(!document["temperature_F"].isNull()){
          assert(m.temperature_c.valid);assert(std::fabs(m.temperature_c.value-(document["temperature_F"].as<double>()-32)*5/9)<0.001);
        }
        if(!document["humidity"].isNull())assert(m.humidity_pct.valid&&m.humidity_pct.time_ms==now);
        if(!document["rain_mm"].isNull())assert(m.rain_counter.valid&&m.rain_unit==preview::RainUnit::Millimetre);
        if(!document["rain_in"].isNull())assert(m.rain_counter.valid&&m.rain_unit==preview::RainUnit::Inch);
      }
      assert(recording.popPacket(record,clock));
      assert(std::string(record.native_json.data(),record.native_json_size)==line);
      assert(record.source==capture::Source::Replay);
    }
  }
  assert(count>0&&live.received==count&&recording.counters().packet_records==count);
  const auto selected_packets=adapter.model().packets;
  constexpr char unknown[]=R"({"model":"Unreviewed-Weather","id":999,"unknown_precision":1.230000,"temperature_C":99})";
  assert(recording.enqueueDecoderOutput(unknown,sizeof(unknown)-1,false,now+1,clock)==capture::PushResult::Queued);
  assert(!adapter.ingest(unknown,sizeof(unknown)-1,now+1));
  assert(adapter.model().packets==selected_packets);
  assert(recording.popPacket(record,clock));
  assert(std::string(record.native_json.data(),record.native_json_size)==unknown);
  std::cout<<"capture/preview replay checks passed: "<<count<<" repeated packets plus unknown model\n";
}
