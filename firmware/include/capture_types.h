#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxconnect::capture {
constexpr size_t kNativeJsonCapacity = 2048;  // Includes the terminating NUL.
constexpr size_t kWorstCaseEscapedNativeBytes = (kNativeJsonCapacity - 1) * 6;
constexpr size_t kQueueDepth = 4;
constexpr size_t kPulsePairLimit = 512;
constexpr size_t kSessionIdChars = 32;
constexpr size_t kProfileIdCapacity = 48;

enum class Source : uint8_t { Live, Synthetic, Replay };
enum class Event : uint8_t { Session, Packet, Pulse, Status, Gap, End };
enum class PushResult : uint8_t { Queued, Inactive, InvalidJson, Oversize, QueueFull, DecoderTruncated };

struct Counters {
  uint64_t packet_records = 0;
  uint64_t pulse_records = 0;
  uint64_t oversize_drops = 0;
  uint64_t invalid_json_drops = 0;
  uint64_t decoder_truncation_drops = 0;
  uint64_t queue_full_drops = 0;
  uint64_t storage_drops = 0;
  uint64_t transport_drops = 0;
};

struct Record {
  Event event = Event::Status;
  Source source = Source::Live;
  std::array<char, kSessionIdChars + 1> session_id{};
  std::array<char, kProfileIdCapacity> profile_id{};
  uint64_t seq = 0;
  uint64_t uptime_ms = 0;
  uint64_t utc_ms = 0;
  bool time_valid = false;
  std::array<char, kNativeJsonCapacity> native_json{};
  uint16_t native_json_size = 0;
  std::array<uint32_t, kPulsePairLimit> pulse_us{}, gap_us{};
  uint32_t original_count = 0, captured_count = 0;
  uint64_t duration_us = 0;
  int32_t signal_rssi_dbm = 0;
  bool pulse_truncated = false, signal_rssi_valid = false;
  Counters capture_counters{};
  uint32_t receiver_drops = 0, raw_callback_drops = 0, raw_callback_skips = 0;
  bool capture_diagnostics_valid = false;
  std::array<char, 32> stop_reason{};
};

struct PulseFrame {
  std::array<uint32_t, kPulsePairLimit> pulse_us{}, gap_us{};
  uint32_t original_count = 0, captured_count = 0;
  uint64_t duration_us = 0;
  int32_t signal_rssi_dbm = 0;
  bool truncated = false, signal_rssi_valid = false;
};

constexpr size_t maxEscapedNativeBytes() { return kWorstCaseEscapedNativeBytes; }
}  // namespace wxconnect::capture
