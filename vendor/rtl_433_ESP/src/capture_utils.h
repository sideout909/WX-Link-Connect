#ifndef RTL_433_ESP_CAPTURE_UTILS_H
#define RTL_433_ESP_CAPTURE_UTILS_H

#include <stdint.h>

namespace rtl_433_capture {

// Unsigned subtraction intentionally preserves Arduino micros() rollover.
constexpr uint32_t elapsedMicros(uint32_t end, uint32_t start) {
  return end - start;
}

// RSSI is sampled by a task, more slowly than OOK symbols. Once a signal is
// active, a cached low RSSI must not discard individual demodulator edges.
constexpr bool acceptPulse(uint32_t duration, uint32_t minimum) {
  return duration > minimum;
}
constexpr bool holdSignal(bool receiving, uint32_t now, uint32_t lastAbove,
                          uint32_t grace) {
  return receiving && elapsedMicros(now, lastAbove) < grace;
}

// Reserve the final array element for the terminating pulse/gap. A full
// train must be dropped explicitly, never wrapped into a plausible short one.
constexpr bool canRecordPulse(unsigned int index, unsigned int capacity) {
  return capacity > 1 && index < capacity - 1;
}

// A completed capture may only advance into an unowned ring slot.
constexpr bool canPublishToNextBuffer(unsigned int nextPulseCount) {
  return nextPulseCount == 0;
}

constexpr bool isCompleteSignal(unsigned int pulseCount, uint32_t duration,
                                unsigned int minimumPulses,
                                uint32_t minimumDuration) {
  return pulseCount > minimumPulses && duration > minimumDuration;
}

} // namespace rtl_433_capture

#endif
