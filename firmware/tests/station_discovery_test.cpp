#include "discovery_packet.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

int main(int argc,char** argv){
  using namespace station_discovery;
  Scan scan;Packet p;p.id=679;p.channel='A';p.fields=Temperature|Humidity|Wind;p.temperature_f=55.2f;p.humidity=74;p.wind_mph=0;
  assert(!scan.observe(p,100));scan.start(100,123);
  for(unsigned t=100;t<6000;t+=100)assert(scan.observe(p,t));
  assert(scan.candidates()[0].sightings==1); // RF burst copies are one sighting.
  assert(!scan.choose(123,1,6000));
  p.fields=Wind|Direction|Rain;scan.observe(p,10100);
  p.fields=Wind|Light|UV;scan.observe(p,20100);
  assert(scan.choose(123,1,20100));assert(!scan.choose(124,1,20100));
  assert(!scan.choose(123,1,140100)); // Every required field has its own age.
  p.fields=127;scan.observe(p,140100);assert(scan.choose(123,1,140100));
  p.channel='B';scan.observe(p,141000);assert(scan.candidates()[1].sightings==1);
  p.model=Model::Iris;scan.observe(p,142000);assert(scan.candidates()[2].packet.model==Model::Iris);
  p.model=Model::Notos;scan.observe(p,143000);scan.observe(p,153000);scan.observe(p,163000);
  assert(scan.candidates()[3].suggested(163000));assert(!scan.candidates()[3].selectable(163000));
  assert(!scan.active(300100));assert(scan.expired(300100));assert(!scan.observe(p,300100));
  scan.cancel();assert(!scan.choose(123,1,150000));scan.start(1,125);assert(!scan.choose(123,1,2));
  for(unsigned i=0;i<kCapacity+5;++i){p.id=i;scan.observe(p,10+i);}
  assert(scan.overflow()==5);assert(scan.candidates()[0].packet.id==0);
  scan.start(UINT32_MAX-10000,126);assert(scan.active(10000));assert(scan.elapsed(10000)==20001);
  StaticJsonDocument<2048>d;
  auto decode=[&](const char* json){assert(!deserializeJson(d,json));return decodeNative(d.as<JsonVariantConst>(),p);};
  assert(decode(R"({"model":"Acurite-Atlas","id":679,"channel":"A","message_type":5,"temperature_F":55.2,"humidity":74,"wind_avg_mi_h":0})"));
  assert(p.fields==(Temperature|Humidity|Wind));
  weather::Sample sample;assert(atlas::decode(d.as<JsonVariantConst>(),100,sample));assert(sample.type==37);
  d["exception"]=1;assert(!decodeNative(d.as<JsonVariantConst>(),p));
  assert(decode(R"({"model":"Acurite-5n1","id":4095,"channel":"B","message_type":56,"temperature_F":55.2,"humidity":74,"wind_avg_km_h":16.09344,"mic":"CHECKSUM"})"));
  assert(p.model==Model::Iris&&p.id==4095&&std::fabs(p.wind_mph-10)<.001f);
  d["mic"]="CRC";assert(!decodeNative(d.as<JsonVariantConst>(),p));
  assert(decode(R"({"model":"Acurite-3n1","id":16383,"channel":"C","message_type":32,"temperature_F":55.2,"humidity":74,"wind_avg_mi_h":0,"mic":"CHECKSUM"})"));
  d["humidity"]=101;assert(!decodeNative(d.as<JsonVariantConst>(),p));
  d["humidity"]=74;d["id"]=16384;assert(!decodeNative(d.as<JsonVariantConst>(),p));
  d["id"]=1;d["channel"]="AB";assert(!decodeNative(d.as<JsonVariantConst>(),p));
  d["channel"]="A";d["wind_avg_mi_h"]=-1;assert(!decodeNative(d.as<JsonVariantConst>(),p));
  d["model"]="Acurite-Tower";assert(!decodeNative(d.as<JsonVariantConst>(),p));
  if(argc>1){
    // Read a private native monitor capture on the host only. Nothing is sent
    // back to a device, history or an external service.
    std::ifstream input(argv[1]);assert(input.good());std::string line;
    Scan captured;uint32_t last=0;unsigned packets=0;
    while(std::getline(input,line)){
      assert(!deserializeJson(d,line));auto native=d["device"];
      if(std::strcmp(native["event"]|"","rf_packet"))continue;
      last=native["uptime_ms"].as<uint32_t>();
      if(!packets)captured.start(last,900);
      assert(decodeNative(native["packet"].as<JsonVariantConst>(),p));captured.observe(p,last);++packets;
    }
    assert(packets>=3&&captured.choose(900,1,last));
    std::cout<<"Native RF capture would suggest a complete Atlas candidate: "<<packets<<" packet copies, "<<captured.candidates()[0].sightings<<" independent sightings.\n";
  }
  std::cout<<"Discovery checks passed: stable weather, independent sightings, identity isolation, per-field freshness, timeout, cancellation, bounded memory, rollover and native profiles.\n";
}
