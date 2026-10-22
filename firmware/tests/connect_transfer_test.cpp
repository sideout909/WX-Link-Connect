#include "connect_transfer.h"
#include <cassert>
#include <vector>
#include <cstring>
#include <iostream>
struct Stream {
  std::vector<uint8_t> bytes;size_t at=0;int fragment=1;
  int available(){return at<bytes.size()?fragment:0;}
  int read(uint8_t* output,size_t count){count=std::min(count,bytes.size()-at);std::memcpy(output,bytes.data()+at,count);at+=count;return int(count);}
};
int main(){
  using namespace wxconnect::transfer;
  Lookup lookup;const auto old=lookup.begin();lookup.cancel(old);
  const auto fresh=lookup.begin();assert(!lookup.complete(old,123));
  assert(lookup.complete(fresh,456)&&lookup.finished&&lookup.address==456);
  assert(!lookup.complete(old,999)&&lookup.address==456);
  Stream stream{{1,2,3,4}};uint8_t bytes[4]{};uint32_t clock=0;
  auto now=[&](){return clock;};auto pause=[&](){++clock;};auto online=[](){return true;};
  assert(readExact(stream,bytes,4,now,pause,online)==ReadResult::Complete);
  assert(bytes[3]==4); // Split image magic/header is valid, one byte at a time.
  Stream empty;clock=0;
  assert(readExact(empty,bytes,4,now,pause,online,100,10)==ReadResult::Timeout&&clock==10);
  assert(readExact(empty,bytes,4,now,pause,[](){return false;})==ReadResult::Closed);
  Stream trickle{{1,2,3,4}};clock=0;
  assert(readExact(trickle,bytes,4,now,[&](){clock+=9;},online,20,10)==ReadResult::Timeout);
  clock=UINT32_MAX-3;Stream wrap{{1,2,3,4}};
  assert(readExact(wrap,bytes,4,now,pause,online)==ReadResult::Complete);
  Stream close_body{{1,2,3,4}};clock=0;size_t length=0;
  assert(readBounded(close_body,bytes,4,length,now,pause,[&](){return close_body.at<close_body.bytes.size();})==ReadResult::Complete && length==4);
  Stream too_big{{1,2,3,4,5}};clock=0;
  assert(readBounded(too_big,bytes,4,length,now,pause,online)==ReadResult::Oversize);
  clock=0;assert(readBounded(empty,bytes,4,length,now,pause,online,20,10)==ReadResult::Timeout);
  const char json_text[]=R"JSON([{"escaped":"}\\\"[","array":[1,2]}])JSON";
  Stream json_stream;json_stream.bytes.assign(json_text,json_text+std::strlen(json_text));clock=0;
  std::vector<uint8_t> json_output(256);JsonBoundary boundary;
  assert(readBounded(json_stream,json_output.data(),json_output.size(),length,now,pause,online,1000,100,&boundary)==ReadResult::Complete);
  assert(boundary.complete&&length==std::strlen(json_text)); // Peer never closes.
  JsonBoundary escaped;const char quoted[]=R"JSON({"text":"}\"["})JSON";
  assert(escaped.append(reinterpret_cast<const uint8_t*>(quoted),std::strlen(quoted))&&escaped.complete);
  JsonBoundary truncated;const uint8_t prefix[]={'[','{'};
  assert(truncated.append(prefix,2)&&!truncated.complete);
  JsonBoundary trailing;const uint8_t extra[]={'[',']','x'};
  assert(!trailing.append(extra,3));
  std::cout<<"Transfer checks passed: split headers, total/idle deadline, closed streams, rollover, late DNS callbacks\n";
}
