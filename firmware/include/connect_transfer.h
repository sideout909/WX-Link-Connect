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
enum class ReadResult { Complete, Timeout, Closed, Error };
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
} // namespace wxconnect::transfer
