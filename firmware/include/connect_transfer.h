#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace wxconnect::transfer {
// Must be called under the platform lock when shared with DNS callbacks.
// Tickets refer to permanent state, never a timed-out task's stack variable.
struct Lookup {
  uint32_t ticket=0,address=0;
  bool pending=false,finished=false;
  uint32_t begin(){if(++ticket==0)++ticket;address=0;pending=true;finished=false;return ticket;}
  bool complete(uint32_t token,uint32_t ip){
    if(token!=ticket||!pending)return false;
    address=ip;pending=false;finished=true;return true;
  }
  void cancel(uint32_t token){if(token==ticket)pending=false;}
};
enum class ReadResult { Complete, Timeout, Closed, Error, Oversize };
// No per-byte blocking readBytes/JSON parse from a network stream. A slow peer
// cannot extend either the total or idle deadline by trickling bytes forever.
template<class Stream,class Clock,class Pause,class Connected>
ReadResult readExact(Stream& stream,uint8_t* output,size_t length,
                     Clock now,Pause pause,Connected connected,
                     uint32_t total_ms=15000,uint32_t idle_ms=3000){
  const uint32_t started=now();uint32_t last=started;size_t used=0;
  while(used<length){
    const uint32_t time=now();
    if(uint32_t(time-started)>=total_ms||uint32_t(time-last)>=idle_ms)return ReadResult::Timeout;
    const int available=stream.available();
    if(available<0)return ReadResult::Error;
    if(available){
      const int got=stream.read(output+used,std::min(size_t(available),length-used));
      if(got<0)return ReadResult::Error;
      if(got){used+=size_t(got);last=now();}
    }else if(!connected())return ReadResult::Closed;
    pause();
  }
  return ReadResult::Complete;
}
// Recognize the boundary of one object/array JSON document without relying on
// a peer promptly closing an HTTP/1.0 socket. The JSON parser still validates
// syntax, types and policy afterward. Quotes/escapes cannot close the frame.
struct JsonBoundary {
  unsigned depth=0;bool quoted=false,escaped=false,started=false,complete=false;
  bool append(const uint8_t* bytes,size_t length){
    for(size_t i=0;i<length;++i){
      const uint8_t c=bytes[i];
      const bool space=c==' '||c=='\r'||c=='\n'||c=='\t';
      if(complete){if(!space)return false;continue;}
      if(!started){if(space)continue;if(c!='{'&&c!='[')return false;started=true;}
      if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
      if(c=='"'){quoted=true;continue;}
      if(c=='{'||c=='['){if(++depth>32)return false;}
      else if(c=='}'||c==']'){if(!depth)return false;if(--depth==0)complete=true;}
    }
    return true;
  }
};
// HTTP/1.0 can delimit a JSON body by connection close instead of Content-Length.
// Capacity, total time and idle time remain bounded for either representation.
template<class Stream,class Clock,class Pause,class Connected>
ReadResult readBounded(Stream& stream,uint8_t* output,size_t capacity,size_t& used,
                       Clock now,Pause pause,Connected connected,
                       uint32_t total_ms=15000,uint32_t idle_ms=3000,JsonBoundary* json=nullptr){
  used=0;const uint32_t started=now();uint32_t last=started;
  for(;;){
    if(uint32_t(now()-started)>=total_ms||uint32_t(now()-last)>=idle_ms)return ReadResult::Timeout;
    const int available=stream.available();
    if(available<0)return ReadResult::Error;
    if(available){
      if(used==capacity)return ReadResult::Oversize;
      const int got=stream.read(output+used,std::min(size_t(available),capacity-used));
      if(got<0)return ReadResult::Error;
      if(got){
        if(json&&!json->append(output+used,size_t(got)))return ReadResult::Error;
        used+=size_t(got);last=now();
        if(json&&json->complete)return ReadResult::Complete;
      }
    }else if(!connected())return json&&!json->complete?ReadResult::Closed:ReadResult::Complete;
    pause();
  }
}
} // namespace wxconnect::transfer
