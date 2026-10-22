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
  std::cout<<"Transfer checks passed: split headers, total/idle deadline, closed streams, rollover, late DNS callbacks\n";
}
