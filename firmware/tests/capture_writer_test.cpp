#include "capture_writer.h"

#include <cassert>
#include <cstdio>

using namespace wxconnect::capture;

int main() {
  ClockMap clock;
  CaptureSession session;
  Record start;
  constexpr char id[] = "0123456789abcdef0123456789abcdef";
  assert(session.start(id, Source::Live, "test", 1, clock, start));
  assert(session.enqueueNative("{}", 2, 2, clock) == PushResult::Queued);
  bool close_called = false;
  const bool stored = appendOrStop(session, Event::Packet, "{}\n", 3,
      [](Event, const char*, size_t) { return false; },
      [&close_called]() { close_called = true; });
  assert(!stored && close_called);
  assert(!session.active() && session.queued() == 0);
  assert(session.counters().storage_drops == 2); // failed record plus queued packet
  std::puts("Capture writer checks passed: rejected storage append closes the session and accounts queued data.");
}
