#pragma once

#include "capture_types.h"

#include <atomic>

namespace wxconnect::capture {
constexpr size_t kPulseQueueDepth = 2;
constexpr uint32_t kRawWindowMinMs = 1000;
constexpr uint32_t kRawWindowMaxMs = 10000;
constexpr uint32_t kRawPulseIntervalMs = 500;

enum class PulsePushResult : uint8_t { Queued, Full, Invalid };

inline bool validRawWindowDuration(uint32_t duration_ms) {
  return duration_ms >= kRawWindowMinMs && duration_ms <= kRawWindowMaxMs;
}

// Single decoder-task producer, main-loop consumer. Source arrays are copied
// before this call returns; neither borrowed pointer is retained.
class PulseQueue {
 public:
  PulsePushResult copyFromCallback(const int* pulse_us, const int* gap_us,
                                  unsigned int count, unsigned long duration_us,
                                  int signal_rssi_dbm) {
    const uint8_t write = write_.load(std::memory_order_relaxed);
    const uint8_t read = read_.load(std::memory_order_acquire);
    if (uint8_t(write - read) >= kPulseQueueDepth) return PulsePushResult::Full;
    if (count && (!pulse_us || !gap_us)) return PulsePushResult::Invalid;
    PulseFrame& frame = frames_[write % kPulseQueueDepth];
    frame = PulseFrame{};
    frame.original_count = count;
    frame.captured_count = count < kPulsePairLimit ? count : kPulsePairLimit;
    frame.truncated = count > frame.captured_count;
    frame.duration_us = duration_us;
    frame.signal_rssi_dbm = signal_rssi_dbm;
    frame.signal_rssi_valid = signal_rssi_dbm >= -150 && signal_rssi_dbm <= 20;
    for (uint32_t i = 0; i < frame.captured_count; ++i) {
      if (pulse_us[i] < 0 || gap_us[i] < 0) {
        frame = PulseFrame{};
        return PulsePushResult::Invalid;
      }
      frame.pulse_us[i] = static_cast<uint32_t>(pulse_us[i]);
      frame.gap_us[i] = static_cast<uint32_t>(gap_us[i]);
    }
    write_.store(uint8_t(write + 1), std::memory_order_release);
    return PulsePushResult::Queued;
  }

  bool pop(PulseFrame& frame) {
    const uint8_t read = read_.load(std::memory_order_relaxed);
    const uint8_t write = write_.load(std::memory_order_acquire);
    if (read == write) return false;
    frame = frames_[read % kPulseQueueDepth];
    frames_[read % kPulseQueueDepth] = PulseFrame{};
    read_.store(uint8_t(read + 1), std::memory_order_release);
    return true;
  }

  size_t size() const {
    return uint8_t(write_.load(std::memory_order_acquire) -
                    read_.load(std::memory_order_acquire));
  }

 private:
  std::array<PulseFrame, kPulseQueueDepth> frames_{};
  std::atomic<uint8_t> write_{0}, read_{0};
};
}  // namespace wxconnect::capture
