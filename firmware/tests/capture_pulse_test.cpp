#include "capture_pulse.h"
#include "capture_jsonl.h"
#include "capture_session.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace wxconnect::capture;

int main() {
  assert(!validRawWindowDuration(999));
  assert(validRawWindowDuration(1000));
  assert(validRawWindowDuration(10000));
  assert(!validRawWindowDuration(10001));

  std::array<int, kPulsePairLimit + 88> pulses{}, gaps{};
  for (size_t i = 0; i < pulses.size(); ++i) {
    pulses[i] = int(i + 1);
    gaps[i] = int(i + 101);
  }
  PulseQueue queue;
  assert(queue.copyFromCallback(pulses.data(), gaps.data(), pulses.size(), 90000, -61) == PulsePushResult::Queued);
  assert(queue.copyFromCallback(pulses.data(), gaps.data(), pulses.size(), 90001, -62) == PulsePushResult::Queued);
  assert(queue.copyFromCallback(pulses.data(), gaps.data(), pulses.size(), 90002, -63) == PulsePushResult::Full);
  pulses.fill(0);
  gaps.fill(0);

  PulseFrame frame;
  assert(queue.pop(frame));
  assert(frame.original_count == pulses.size() && frame.captured_count == kPulsePairLimit);
  assert(frame.truncated && frame.duration_us == 90000 && frame.signal_rssi_valid);
  assert(frame.signal_rssi_dbm == -61 && frame.pulse_us[0] == 1 && frame.gap_us[0] == 101);
  assert(queue.pop(frame) && frame.signal_rssi_dbm == -62);
  assert(!queue.pop(frame));

  int bad_pulse[] = {100, -1};
  int good_gap[] = {20, 30};
  assert(queue.copyFromCallback(bad_pulse, good_gap, 2, 10, -40) == PulsePushResult::Invalid);
  assert(queue.size() == 0);

  int exact_pulses[kPulsePairLimit], exact_gaps[kPulsePairLimit];
  for (size_t i = 0; i < kPulsePairLimit; ++i) { exact_pulses[i] = 200; exact_gaps[i] = 300; }
  assert(queue.copyFromCallback(exact_pulses, exact_gaps, kPulsePairLimit, 100000, -70) == PulsePushResult::Queued);
  assert(queue.pop(frame) && !frame.truncated && frame.captured_count == kPulsePairLimit);

  ClockMap clock;
  CaptureSession session;
  Record record;
  constexpr char id[] = "0123456789abcdef0123456789abcdef";
  assert(session.start(id, Source::Live, "ook433-baseline-v1", 1, clock, record));
  assert(session.makePulseRecord(frame, 2, clock, record));
  assert(record.event == Event::Pulse && record.seq == 1 && record.captured_count == kPulsePairLimit);
  assert(session.counters().pulse_records == 1);
  std::string line(kPulsePairLimit * 24 + 1024, '\0');
  const size_t encoded = encodeJsonl(record, line.data(), line.size());
  assert(encoded > 0 && detail::JsonValidator(line.data(), encoded - 1).valid());
  assert(line.find("\"packet_seq\":null") != std::string::npos);
  assert(line.find("\"signal_rssi_dbm\":-70") != std::string::npos);
  assert(line.find("\"original_count\":512") != std::string::npos);
  Record end;
  assert(session.stop(3, clock, end));
  assert(!session.makePulseRecord(frame, 4, clock, record));
  std::puts("Pulse capture checks passed: bounded windows/queue, callback copy lifetime, cap/truncation, validation, JSONL and stop behavior.");
}
