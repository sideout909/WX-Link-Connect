#include "capture_session.h"
extern "C" {
#include "data.h"
}

#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>

using namespace wxconnect::capture;

int main() {
  const std::string payload(6000, 'x');
  data_t* native = data_make("model", "", DATA_STRING, "test",
                             "unknown", "", DATA_STRING, payload.c_str(), NULL);
  assert(native);

  char small[1024]{};
  int truncated = 0;
  const size_t partial_size = data_print_jsons_checked(native, small, sizeof(small), &truncated);
  assert(truncated == 1);
  assert(partial_size > 0 && partial_size < sizeof(small));
  const int small_truncated = truncated;

  char sufficient[8192]{};
  truncated = 1;
  const size_t full_size = data_print_jsons_checked(native, sufficient,
                                                      sizeof(sufficient), &truncated);
  assert(truncated == 0);
  assert(full_size > partial_size && full_size < sizeof(sufficient));
  data_free(native);

  ClockMap clock;
  CaptureSession session;
  Record record;
  constexpr char id[] = "0123456789abcdef0123456789abcdef";
  assert(session.start(id, Source::Live, "ook-433920-v1", 1, clock, record));
  assert(session.enqueueDecoderOutput(small, partial_size, small_truncated != 0, 2, clock) ==
         PushResult::DecoderTruncated);
  assert(session.counters().decoder_truncation_drops == 1);
  assert(session.counters().packet_records == 0);
  assert(!session.popPacket(record, clock));
  std::puts("Decoder formatter checks passed: small output reports truncation; loss is counted without a packet record.");
}
