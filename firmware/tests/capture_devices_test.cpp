#include "capture_devices.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace wxconnect::capture;

int main() {
  CandidateTable table;
  assert(table.observe("rtl_433", "AcuRite-Atlas", "17", "A", 100));
  assert(table.observe("rtl_433", "AcuRite-Atlas", "17", "A", 200));
  assert(table.observe("rtl_433", "AcuRite-Atlas", "18", "A", 300));
  assert(table.observe("rtl_433", "AcuRite-Atlas", "17", "B", 400));
  assert(table.observe("rtl_433", "Unlisted-Weather", nullptr, nullptr, 500));
  assert(table.observe("rtl_433", "Unlisted-Weather", nullptr, nullptr, 600));

  const auto& entries = table.entries();
  assert(entries[0].packets == 2 && entries[0].complete());
  assert(entries[0].last_uptime_ms == 200);
  assert(entries[1].has_id && std::string(entries[1].id.data()) == "18");
  assert(entries[2].has_channel && std::string(entries[2].channel.data()) == "B");
  assert(entries[3].packets == 2 && !entries[3].has_id && !entries[3].complete());

  char id[16];
  for (unsigned i = 0; i < kCandidateCapacity - 4; ++i) {
    std::snprintf(id, sizeof(id), "x%u", i);
    assert(table.observe("p", "m", id, nullptr, 700 + i));
  }
  assert(!table.observe("overflow", "unknown", "extra", nullptr, 900));
  assert(table.overflow() == 1);
  const std::string too_long(kIdentityFieldCapacity, 'x');
  assert(!table.observe(too_long.c_str(), "m", "id", nullptr, 901));
  assert(table.oversize() == 1);
  std::puts("Candidate table checks passed: identities, unknown IDs, repeats, bounds and overflow.");
}
