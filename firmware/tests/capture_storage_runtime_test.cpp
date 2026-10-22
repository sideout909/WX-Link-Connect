#include "../src/capture_storage.cpp"
#include <cassert>
#include <iostream>
#include <unistd.h>
using namespace wxconnect::capture;
namespace storage=wxconnect::capture::storage;
int main(){
  char temp[]="/tmp/wxc-storage-XXXXXX";assert(mkdtemp(temp));fs_test::root=temp;
  constexpr char id[]="0123456789abcdef0123456789abcdef";
  constexpr char packet[]="{\"event\":\"packet\",\"synthetic\":true}\n";
  constexpr char end[]="{\"event\":\"end\"}\n";
  assert(storage::begin());
  // The old 44-character filename fails under the exact 32-character image limit.
  assert(!LittleFS.open(String("/connect/captures/")+id+".decoded.wxc","w"));
  assert(storage::beginSession(id));
  assert(storage::append(Event::Packet,packet,sizeof(packet)-1));
  assert(storage::append(Event::Pulse,packet,sizeof(packet)-1));
  assert(storage::append(Event::End,end,sizeof(end)-1));
  assert(storage::finish());assert(storage::state().sessions==1);
  assert(storage::recover()); // Reboot recovery, both streams and real byte sizes.
  assert(storage::sessions()[0].decoded_records==2);
  assert(storage::sessions()[0].pulse_records==1);
  assert(!storage::sessions()[0].interrupted);
  char out[128];size_t length=0;uint32_t next=0;
  assert(storage::readRecord(id,true,0,out,sizeof(out),length,next));
  assert(length==sizeof(packet)-1&&!std::memcmp(out,packet,length));
  assert(storage::deleteSession(id));assert(storage::state().sessions==0);
  assert(storage::beginSession(id));
  assert(storage::append(Event::Packet,packet,sizeof(packet)-1));
  fs_test::reject_writes=true;assert(!storage::finish());fs_test::reject_writes=false;
  assert(!storage::state().active);assert(storage::deleteSession(id));
  // Legacy flat sessions remain readable, including pulse-only recovery.
  fs_test::limit_names=false;
  assert(storage::beginSession(id));assert(storage::append(Event::Pulse,packet,sizeof(packet)-1));assert(storage::finish());
  auto folder=fs_test::root/"connect/captures"/id;
  std::filesystem::rename(folder/"pulses.wxc",fs_test::root/"connect/captures"/(std::string(id)+".pulses.wxc"));
  std::filesystem::remove_all(folder);
  assert(storage::recover());assert(storage::sessions()[0].pulse_records==1);
  assert(storage::deleteSession(id));
  std::filesystem::remove_all(fs_test::root);
  std::cout<<"Capture storage runtime checks passed (32-character components, recovery, reads, deletion, write failure, legacy)\n";
}
