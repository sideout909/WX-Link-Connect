#include "setup_restart.h"
#include "connect_live.h"
#include <cstring>
#include <cassert>
#include <limits>
int main() {
  wxconnect::LivePacketView view;
  char packet[]="{\"unknown\":1.2300,\"name\":\"<img src=x>\"}";
  assert(view.observe(packet,std::strlen(packet),false,12));
  assert(view.received==1 && view.received_at==12);
  packet[12]='9'; // The retained view must own a copy.
  assert(view.json[12]!='9');
  char repeat[2048]; std::strcpy(repeat,view.json.data());
  assert(view.observe(repeat,std::strlen(repeat),false,13));
  assert(view.received==2); // Repeated values are still received packets.
  assert(!view.observe("{}",2,true,14));
  assert(!view.observe("{",1,false,15));
  char huge[2050]{};
  assert(!view.observe(huge,sizeof(huge),false,16));
  assert(view.received==2 && view.rejected==3 && view.received_at==13);
  wxconnect::AutomaticCapture automatic;
  assert(!automatic.claim(false,true,true,false,false));
  assert(!automatic.claim(true,false,true,false,false));
  assert(!automatic.claim(true,true,false,false,false));
  assert(!automatic.claim(true,true,true,true,false));
  assert(automatic.claim(true,true,true,false,false));
  assert(!automatic.claim(true,false,true,false,false));
  assert(!automatic.claim(true,true,true,false,false)); // Reconnect must not consume another retained slot.
  assert(!automatic.durationExpired(3599999,0));
  assert(automatic.durationExpired(3600000,0));
  wxconnect::AutomaticCapture manual;
  assert(!manual.claim(true,true,true,false,true));
  assert(!manual.claim(true,true,true,false,false)); // Stop must remain stopped.
  wxconnect::SetupRestart restart;
  assert(!restart.pending() && !restart.due(100000));
  assert(!restart.schedule(0,true));
  assert(!restart.pending());
  assert(restart.schedule(100,false));
  assert(restart.pending() && !restart.due(2599));
  assert(restart.due(2600));
  assert(!restart.schedule(2600,false));
  assert(restart.due(2601));
  wxconnect::SetupRestart long_uptime;
  const uint64_t start=uint64_t(std::numeric_limits<uint32_t>::max())-100;
  assert(long_uptime.schedule(start,false));
  assert(!long_uptime.due(start+2499));
  assert(long_uptime.due(start+2500));
}
