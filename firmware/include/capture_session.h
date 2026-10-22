#pragma once

#include "capture_types.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>

namespace wxconnect::capture {
inline void formatSessionId(const uint8_t random_bytes[16], char output[kSessionIdChars + 1]) {
  static constexpr char hex[] = "0123456789abcdef";
  if (!random_bytes || !output) return;
  for (size_t i = 0; i < 16; ++i) {
    output[i * 2] = hex[random_bytes[i] >> 4];
    output[i * 2 + 1] = hex[random_bytes[i] & 0x0f];
  }
  output[kSessionIdChars] = '\0';
}

namespace detail {
class JsonValidator {
 public:
  JsonValidator(const char* bytes, size_t size) : bytes_(bytes), size_(size) {}

  bool valid() {
    if (!bytes_ || !size_) return false;
    skipSpace();
    if (!value(0)) return false;
    skipSpace();
    return at_ == size_;
  }

 private:
  bool value(unsigned depth) {
    if (depth > 16 || at_ >= size_) return false;
    switch (bytes_[at_]) {
      case '{': return object(depth + 1);
      case '[': return array(depth + 1);
      case '"': return string();
      case 't': return literal("true");
      case 'f': return literal("false");
      case 'n': return literal("null");
      default: return number();
    }
  }

  bool object(unsigned depth) {
    ++at_;
    skipSpace();
    if (take('}')) return true;
    for (;;) {
      if (!string()) return false;
      skipSpace();
      if (!take(':')) return false;
      skipSpace();
      if (!value(depth)) return false;
      skipSpace();
      if (take('}')) return true;
      if (!take(',')) return false;
      skipSpace();
    }
  }

  bool array(unsigned depth) {
    ++at_;
    skipSpace();
    if (take(']')) return true;
    for (;;) {
      if (!value(depth)) return false;
      skipSpace();
      if (take(']')) return true;
      if (!take(',')) return false;
      skipSpace();
    }
  }

  bool string() {
    if (!take('"')) return false;
    while (at_ < size_) {
      const unsigned char ch = static_cast<unsigned char>(bytes_[at_++]);
      if (ch == '"') return true;
      if (ch < 0x20) return false;
      if (ch != '\\') continue;
      if (at_ >= size_) return false;
      const char escape = bytes_[at_++];
      if (escape == '"' || escape == '\\' || escape == '/' || escape == 'b' ||
          escape == 'f' || escape == 'n' || escape == 'r' || escape == 't') continue;
      if (escape != 'u' || size_ - at_ < 4) return false;
      for (unsigned i = 0; i < 4; ++i) {
        const unsigned char hex = static_cast<unsigned char>(bytes_[at_++]);
        if (!std::isxdigit(hex)) return false;
      }
    }
    return false;
  }

  bool number() {
    const size_t start = at_;
    take('-');
    if (take('0')) {
      if (at_ < size_ && std::isdigit(static_cast<unsigned char>(bytes_[at_]))) return false;
    } else {
      if (at_ >= size_ || bytes_[at_] < '1' || bytes_[at_] > '9') return false;
      while (at_ < size_ && std::isdigit(static_cast<unsigned char>(bytes_[at_]))) ++at_;
    }
    if (take('.')) {
      const size_t digits = at_;
      while (at_ < size_ && std::isdigit(static_cast<unsigned char>(bytes_[at_]))) ++at_;
      if (digits == at_) return false;
    }
    if (take('e') || take('E')) {
      if (!take('+')) take('-');
      const size_t digits = at_;
      while (at_ < size_ && std::isdigit(static_cast<unsigned char>(bytes_[at_]))) ++at_;
      if (digits == at_) return false;
    }
    return at_ != start;
  }

  bool literal(const char* text) {
    const size_t length = std::strlen(text);
    if (size_ - at_ < length || std::memcmp(bytes_ + at_, text, length)) return false;
    at_ += length;
    return true;
  }

  void skipSpace() {
    while (at_ < size_ && (bytes_[at_] == ' ' || bytes_[at_] == '\t' ||
                           bytes_[at_] == '\n' || bytes_[at_] == '\r')) ++at_;
  }

  bool take(char expected) {
    if (at_ >= size_ || bytes_[at_] != expected) return false;
    ++at_;
    return true;
  }

  const char* bytes_;
  size_t size_;
  size_t at_ = 0;
};
}  // namespace detail

class MonotonicMillis {
 public:
  uint64_t observe(uint32_t millis_value) {
    if (seen_ && millis_value < last_ && uint32_t(last_ - millis_value) > 0x80000000u)
      high_ += uint64_t{1} << 32;
    last_ = millis_value;
    seen_ = true;
    return high_ + millis_value;
  }

 private:
  uint32_t last_ = 0;
  uint64_t high_ = 0;
  bool seen_ = false;
};

class ClockMap {
 public:
  uint64_t observe(uint32_t raw_uptime_ms) { return monotonic_.observe(raw_uptime_ms); }

  bool synchronize(uint32_t raw_uptime_ms, uint64_t epoch_ms) {
    const uint64_t uptime = observe(raw_uptime_ms);
    if (epoch_ms < 1700000000000ULL) return false;
    anchor_uptime_ms_ = uptime;
    anchor_utc_ms_ = epoch_ms;
    valid_ = true;
    return true;
  }

  bool utcAt(uint64_t uptime_ms, uint64_t& epoch_ms) const {
    if (!valid_ || uptime_ms < anchor_uptime_ms_) return false;
    const uint64_t delta = uptime_ms - anchor_uptime_ms_;
    if (delta > std::numeric_limits<uint64_t>::max() - anchor_utc_ms_) return false;
    epoch_ms = anchor_utc_ms_ + delta;
    return true;
  }

 private:
  MonotonicMillis monotonic_;
  uint64_t anchor_uptime_ms_ = 0;
  uint64_t anchor_utc_ms_ = 0;
  bool valid_ = false;
};

class CaptureSession {
 public:
  bool start(const char* session_id, Source source, const char* profile_id,
             uint32_t raw_uptime_ms, ClockMap& clock, Record& session_record) {
    if (active_ || !validId(session_id) || !validProfile(profile_id)) return false;
    clearQueue();
    counters_ = Counters{};
    source_ = source;
    std::memcpy(session_id_.data(), session_id, kSessionIdChars);
    session_id_[kSessionIdChars] = '\0';
    profile_id_.fill('\0');
    std::strncpy(profile_id_.data(), profile_id, profile_id_.size() - 1);
    next_seq_ = 0;
    active_ = true;
    fillCommon(session_record, Event::Session, clock.observe(raw_uptime_ms), clock);
    session_record.seq = next_seq_++;
    return true;
  }

  PushResult enqueueNative(const char* bytes, size_t size, uint32_t raw_uptime_ms,
                           ClockMap& clock) {
    if (!active_) return PushResult::Inactive;
    if (!bytes || size + 1 > kNativeJsonCapacity || size > UINT16_MAX) {
      ++counters_.oversize_drops;
      return PushResult::Oversize;
    }
    if (!detail::JsonValidator(bytes, size).valid()) {
      ++counters_.invalid_json_drops;
      return PushResult::InvalidJson;
    }
    if (queued_ == kQueueDepth) {
      ++counters_.queue_full_drops;
      return PushResult::QueueFull;
    }
    QueuedPacket& packet = queue_[write_];
    packet.bytes.fill('\0');
    std::memcpy(packet.bytes.data(), bytes, size);
    packet.size = static_cast<uint16_t>(size);
    packet.uptime_ms = clock.observe(raw_uptime_ms);
    write_ = (write_ + 1) % kQueueDepth;
    ++queued_;
    return PushResult::Queued;
  }

  PushResult enqueueDecoderOutput(const char* bytes, size_t size, bool truncated,
                                 uint32_t raw_uptime_ms, ClockMap& clock) {
    if (!active_) return PushResult::Inactive;
    if (truncated) {
      ++counters_.decoder_truncation_drops;
      return PushResult::DecoderTruncated;
    }
    return enqueueNative(bytes, size, raw_uptime_ms, clock);
  }

  bool popPacket(Record& record, ClockMap& clock) {
    if (!queued_) return false;
    const QueuedPacket& packet = queue_[read_];
    fillCommon(record, Event::Packet, packet.uptime_ms, clock);
    record.native_json = packet.bytes;
    record.native_json_size = packet.size;
    record.seq = next_seq_++;
    ++counters_.packet_records;
    read_ = (read_ + 1) % kQueueDepth;
    --queued_;
    return true;
  }

  bool makePulseRecord(const PulseFrame& frame, uint32_t raw_uptime_ms,
                       ClockMap& clock, Record& record) {
    if (!active_ || frame.captured_count > kPulsePairLimit ||
        frame.captured_count > frame.original_count) return false;
    fillCommon(record, Event::Pulse, clock.observe(raw_uptime_ms), clock);
    record.original_count = frame.original_count;
    record.captured_count = frame.captured_count;
    record.pulse_truncated = frame.truncated;
    record.duration_us = frame.duration_us;
    record.signal_rssi_dbm = frame.signal_rssi_dbm;
    record.signal_rssi_valid = frame.signal_rssi_valid;
    std::copy_n(frame.pulse_us.begin(), frame.captured_count, record.pulse_us.begin());
    std::copy_n(frame.gap_us.begin(), frame.captured_count, record.gap_us.begin());
    record.seq = next_seq_++;
    ++counters_.pulse_records;
    return true;
  }

  bool stop(uint32_t raw_uptime_ms, ClockMap& clock, Record& end_record) {
    if (!active_ || queued_) return false;
    active_ = false;
    fillCommon(end_record, Event::End, clock.observe(raw_uptime_ms), clock);
    end_record.seq = next_seq_++;
    end_record.capture_counters = counters_;
    end_record.capture_diagnostics_valid = true;
    std::strncpy(end_record.stop_reason.data(), "requested", end_record.stop_reason.size() - 1);
    return true;
  }

  void abortForStorageFailure() {
    if (!active_) return;
    counters_.storage_drops += queued_;
    clearQueue();
    active_ = false;
  }

  void storageDrop() { ++counters_.storage_drops; }
  void transportDrop() { ++counters_.transport_drops; }
  bool active() const { return active_; }
  const char* sessionId() const { return session_id_.data(); }
  size_t queued() const { return queued_; }
  const Counters& counters() const { return counters_; }

 private:
  struct QueuedPacket {
    std::array<char, kNativeJsonCapacity> bytes{};
    uint16_t size = 0;
    uint64_t uptime_ms = 0;
  };

  static bool validId(const char* id) {
    if (!id || std::strlen(id) != kSessionIdChars) return false;
    for (size_t i = 0; i < kSessionIdChars; ++i)
      if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
    return true;
  }

  static bool validProfile(const char* profile) {
    if (!profile || !profile[0] || std::strlen(profile) >= kProfileIdCapacity) return false;
    return true;
  }

  void fillCommon(Record& record, Event event, uint64_t uptime_ms, ClockMap& clock) {
    record = Record{};
    record.event = event;
    record.source = source_;
    record.session_id = session_id_;
    record.profile_id = profile_id_;
    record.uptime_ms = uptime_ms;
    record.time_valid = clock.utcAt(uptime_ms, record.utc_ms);
    if (!record.time_valid) record.utc_ms = 0;
  }

  void clearQueue() {
    queue_ = {};
    read_ = 0;
    write_ = 0;
    queued_ = 0;
  }

  std::array<QueuedPacket, kQueueDepth> queue_{};
  std::array<char, kSessionIdChars + 1> session_id_{};
  std::array<char, kProfileIdCapacity> profile_id_{};
  Counters counters_{};
  uint64_t next_seq_ = 0;
  size_t read_ = 0, write_ = 0, queued_ = 0;
  Source source_ = Source::Live;
  bool active_ = false;
};
}  // namespace wxconnect::capture
