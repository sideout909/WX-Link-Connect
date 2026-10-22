#pragma once

#include "capture_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxconnect::capture::storage {
constexpr size_t kSessionLimit = 8;
constexpr size_t kPoolBytes = 256 * 1024;
constexpr size_t kFilesystemReserveBytes = 512 * 1024;

struct SessionInfo {
  std::array<char, kSessionIdChars + 1> id{};
  uint32_t decoded_bytes = 0, pulse_bytes = 0;
  uint32_t decoded_records = 0, pulse_records = 0;
  uint32_t decoded_valid_bytes = 0, pulse_valid_bytes = 0;
  bool occupied = false, interrupted = false, corrupt = false, incomplete_tail = false;
};

struct State {
  bool ready = false, active = false;
  const char* start_blocker = nullptr;
  uint8_t sessions = 0, recovered_sessions = 0;
  uint64_t total_bytes = 0, used_bytes = 0, free_bytes = 0;
  uint32_t storage_drops = 0, crc_failures = 0;
};

bool begin();
bool beginSession(const char* session_id);
bool append(Event event, const char* jsonl, size_t length);
bool tick();
bool flush();
bool finish();
bool deleteSession(const char* session_id);
bool readRecord(const char* session_id, bool pulses, uint32_t offset,
                char* output, size_t capacity, size_t& length, uint32_t& next_offset);
const std::array<SessionInfo, kSessionLimit>& sessions();
State state();
// Borrowed diagnostic string; valid until the next storage operation.
const char* statusMessage();
}  // namespace wxconnect::capture::storage
