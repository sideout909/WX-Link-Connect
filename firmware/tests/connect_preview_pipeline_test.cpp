#include "connect_preview.h"
#include "connect_live.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
using namespace wxconnect;
// Synthetic packets follow the actual vendored Atlas decoder schema: no protocol.
static const char* packets[] = {
 R"({"model":"Acurite-Atlas","id":101,"channel":"A","sequence_num":0,"message_type":5,"wind_avg_mi_h":4.0,"temperature_F":68.0,"humidity":55,"raw_msg":"synthetic"})",
 R"({"model":"Acurite-Atlas","id":101,"channel":"A","sequence_num":1,"message_type":6,"wind_avg_mi_h":4.0,"wind_dir_deg":180.0,"rain_in":1.23,"raw_msg":"synthetic"})",
 R"({"model":"Acurite-Atlas","id":101,"channel":"A","sequence_num":2,"message_type":7,"wind_avg_mi_h":4.0,"uv":2,"lux":12340,"raw_msg":"synthetic"})"
};
int main() {
  { // The live library appends its full decoder name, not just a numeric protocol.
    constexpr char name[]="Acurite 592TXR Temp/Humidity, 592TX Temp, 5n1 Weather Station, 6045 Lightning, 899 Rain, 3N1, Atlas";
    const std::string packet=std::string("{\"protocol\":\"")+name+"\",\"model\":\"Acurite-Atlas\",\"id\":101,\"channel\":\"A\",\"temperature_C\":20}";
    capture::CandidateTable table;preview::Adapter actual;
    for(unsigned i=0;i<3;++i) assert(table.observe(name,"Acurite-Atlas","101","A",i));
    const auto* choice=preview::automaticCandidate(table,actual);assert(choice);
    assert(actual.select(choice->protocol.data(),choice->model.data(),choice->id.data(),choice->channel.data()));
    assert(actual.ingest(packet.c_str(),packet.size(),4));
    assert(actual.model().temperature_c.valid && actual.model().temperature_c.value==20);
  }

  // Reproduce 0.1.1: zero-copy parsing silently destroys the borrowed JSON text.
  char broken[2048];std::strcpy(broken,packets[0]);
  StaticJsonDocument<2048> old_document;
  assert(!deserializeJson(old_document,broken));
  assert(std::strcmp(broken,packets[0])!=0);
  preview::Adapter old_adapter;old_adapter.select(nullptr,"Acurite-Atlas","101","A");
  assert(!old_adapter.ingest(broken,std::strlen(packets[0]),1));

  preview::Adapter adapter;capture::CandidateTable candidates;
  capture::CaptureSession recording;capture::ClockMap clock;capture::Record record;
  assert(recording.start("0123456789abcdef0123456789abcdef",capture::Source::Synthetic,"synthetic",0,clock,record));
  LivePacketView live;
  for (unsigned i=0;i<6;++i) {
    const char* original=packets[i%3];char packet[2048];std::strcpy(packet,original);
    const auto size=std::strlen(packet);const unsigned now=(i+1)*1000;
    assert(recording.enqueueDecoderOutput(packet,size,false,now,clock)==capture::PushResult::Queued);
    assert(live.observe(packet,size,false,now));
    StaticJsonDocument<2048> document;
    assert(preview::parseNative(packet,size,document));
    assert(std::strcmp(packet,original)==0); // Actual broker parser must own strings.
    assert(document["protocol"].isNull());
    assert(candidates.observe(nullptr,document["model"],"101",document["channel"],now));
    if (const auto* choice=preview::automaticCandidate(candidates,adapter))
      assert(adapter.select(nullptr,choice->model.data(),choice->id.data(),choice->channel.data()));
    adapter.ingest(packet,size,now);
    assert(recording.popPacket(record,clock));
    assert(std::string(record.native_json.data(),record.native_json_size)==original);
  }
  assert(live.received==6 && adapter.model().selected);
  const auto& model=adapter.model();
  assert(model.temperature_c.valid && std::fabs(model.temperature_c.value-20)<.001);
  assert(model.humidity_pct.valid && model.humidity_pct.value==55);
  assert(model.wind_mph.valid && model.wind_mph.value==4);
  assert(model.wind_direction_deg.valid && model.wind_direction_deg.value==180);
  assert(model.rain_counter.valid && model.rain_counter.value==1.23);
  assert(model.light_lux.valid && model.light_lux.value==12340);
  assert(model.uv_index.valid && model.uv_index.value==2);
  assert(model.temperature_c.time_ms==4000 && model.light_lux.time_ms==6000);
  candidates.observe("unknown","Other-Station","202",nullptr,7000);
  assert(!preview::automaticCandidate(candidates,adapter));
  assert(adapter.model().temperature_c.valid); // Neighbor does not blank Home.
  constexpr char neighbor[]=R"({"model":"Acurite-Atlas","id":202,"channel":"A","temperature_F":100})";
  assert(!adapter.ingest(neighbor,sizeof(neighbor)-1,7000));
  assert(adapter.model().temperature_c.value==20);
  preview::Adapter undecided;capture::CandidateTable competing;
  for(unsigned i=0;i<3;++i)competing.observe(nullptr,"Acurite-Atlas","101","A",i);
  competing.observe(nullptr,"Acurite-5n1","202","B",4);
  assert(!preview::automaticCandidate(competing,undecided));
  preview::Adapter fresh;capture::CandidateTable mixed;
  mixed.observe("unknown","Other-Station","202",nullptr,1);
  for(int i=0;i<3;++i)mixed.observe(nullptr,"Acurite-Atlas","101","A",i+2);
  const auto* choice=preview::automaticCandidate(mixed,fresh);
  assert(choice && choice->id[0]=='1'); // Unknown neighbor cannot prevent Atlas selection.
}
