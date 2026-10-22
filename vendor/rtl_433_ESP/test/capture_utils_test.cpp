#include <assert.h>
#include <stdint.h>

#include "capture_utils.h"

int main() {
  using namespace rtl_433_capture;

  assert(elapsedMicros(150, 100) == 50);
  assert(elapsedMicros(0x00000010u, 0xfffffff0u) == 32);
  // Early OOK dropout: the old >30ms condition ended this valid signal at 1ms.
  assert(holdSignal(true, 1000, 500, 40000));
  assert(!holdSignal(false, 1000, 500, 40000));
  assert(!holdSignal(true, 40500, 500, 40000));
  assert(holdSignal(true, 16, 0xfffffff0u, 40000));
  assert(!acceptPulse(50, 50));
  // A cached RSSI low interval must not delete alternating 200/400us symbols.
  for (unsigned i=0;i<80;++i) assert(acceptPulse(i%2?200:400,50));
  assert(canRecordPulse(1198,1200));
  assert(!canRecordPulse(1199,1200));
  unsigned index=0,loss=0;bool overflow=false;
  for(unsigned edge=0;edge<1600;++edge){
    if(!canRecordPulse(index,1200)){if(!overflow)++loss;overflow=true;continue;}
    ++index;
  }
  assert(overflow && index==1199&&loss==1); // Never wraps into a supposedly complete packet.
  assert(canPublishToNextBuffer(0));
  assert(!canPublishToNextBuffer(1));
  assert(isCompleteSignal(17, 40001, 16, 40000));
  assert(!isCompleteSignal(16, 40001, 16, 40000));
  assert(!isCompleteSignal(17, 40000, 16, 40000));
  return 0;
}
