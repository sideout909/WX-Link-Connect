#pragma once

#include "capture_types.h"
#include "connect_decoder_profile.h"
#include "firmware_version.h"

#include <cstdio>
#include <cstring>

namespace wxconnect::capture {
namespace detail {
class BoundedJsonWriter {
 public:
  BoundedJsonWriter(char* output, size_t capacity) : output_(output), capacity_(capacity) {}
  bool literal(const char* value) { return bytes(value, std::strlen(value)); }
  bool bytes(const char* value, size_t length) {
    if (!output_ || used_ > capacity_ || length >= capacity_ - used_) return false;
    std::memcpy(output_ + used_, value, length);
    used_ += length;
    output_[used_] = '\0';
    return true;
  }
  bool string(const char* value, size_t length) {
    static constexpr char hex[] = "0123456789abcdef";
    if (!literal("\"")) return false;
    for (size_t i = 0; i < length; ++i) {
      const unsigned char ch = static_cast<unsigned char>(value[i]);
      if (ch == '"' || ch == '\\') {
        const char escaped[] = {'\\', char(ch)};
        if (!bytes(escaped, sizeof(escaped))) return false;
      } else if (ch < 0x20) {
        const char escaped[] = {'\\', 'u', '0', '0', hex[ch >> 4], hex[ch & 0x0f]};
        if (!bytes(escaped, sizeof(escaped))) return false;
      } else if (!bytes(reinterpret_cast<const char*>(&value[i]), 1)) return false;
    }
    return literal("\"");
  }
  bool number(uint64_t value) {
    char digits[24];
    const int size = std::snprintf(digits, sizeof(digits), "%llu",
                                   static_cast<unsigned long long>(value));
    return size > 0 && size_t(size) < sizeof(digits) && bytes(digits, size_t(size));
  }
  bool signedNumber(int32_t value) {
    char digits[16];
    const int size = std::snprintf(digits, sizeof(digits), "%ld", static_cast<long>(value));
    return size > 0 && size_t(size) < sizeof(digits) && bytes(digits, size_t(size));
  }
  size_t size() const { return used_; }

 private:
  char* output_;
  size_t capacity_;
  size_t used_ = 0;
};
}  // namespace detail

inline const char* eventName(Event event) {
  switch (event) {
    case Event::Session: return "session";
    case Event::Packet: return "packet";
    case Event::Pulse: return "pulse";
    case Event::Status: return "status";
    case Event::Gap: return "gap";
    case Event::End: return "end";
  }
  return "status";
}

inline const char* sourceName(Source source) {
  switch (source) {
    case Source::Live: return "live";
    case Source::Synthetic: return "synthetic";
    case Source::Replay: return "replay";
  }
  return "live";
}

inline size_t encodeJsonl(const Record& record, char* output, size_t capacity) {
  if (!output || !capacity) return 0;
  detail::BoundedJsonWriter writer(output, capacity);
  bool ok = writer.literal("{\"schema\":1,\"event\":\"") &&
      writer.literal(eventName(record.event)) && writer.literal("\",\"session_id\":") &&
      writer.string(record.session_id.data(), kSessionIdChars) &&
      writer.literal(",\"seq\":") && writer.number(record.seq) &&
      writer.literal(",\"uptime_ms\":") && writer.number(record.uptime_ms) &&
      writer.literal(",\"utc_ms\":");
  if (ok) ok = record.time_valid ? writer.number(record.utc_ms) : writer.literal("null");
  if (ok) ok = writer.literal(record.time_valid
      ? ",\"time_valid\":true,\"source\":\""
      : ",\"time_valid\":false,\"source\":\"");
  if (ok) ok = writer.literal(sourceName(record.source)) &&
      writer.literal("\",\"profile_id\":") &&
      writer.string(record.profile_id.data(), strnlen(record.profile_id.data(), record.profile_id.size()));
  if (ok && record.event == Event::Session) {
    ok = writer.literal(",\"decoder_library\":") &&
         writer.string(wxconnect::radio_profile::kLibrary, std::strlen(wxconnect::radio_profile::kLibrary)) &&
         writer.literal(",\"decoder_revision\":") &&
         writer.string(wxconnect::radio_profile::kRevision, std::strlen(wxconnect::radio_profile::kRevision)) &&
         writer.literal(",\"decoder_local_receiver_revision\":") &&
         writer.string(wxconnect::radio_profile::kLocalReceiverRevision,
                       std::strlen(wxconnect::radio_profile::kLocalReceiverRevision)) &&
         writer.literal(",\"profile_revision\":") && writer.number(wxconnect::radio_profile::kRevisionNumber) &&
         writer.literal(",\"radio_initialization_state\":\"init_accepted_readback_unverified\",\"requested_center_frequency_mhz\":433.92,\"requested_modulation\":\"OOK\",\"requested_rx_bandwidth_khz\":812,\"requested_rssi_margin_db\":9,\"radio_readback_verified\":false,\"firmware\":") &&
         writer.string(firmware_version::display, std::strlen(firmware_version::display)) &&
         writer.literal(",\"board\":") && writer.string(firmware_version::board, std::strlen(firmware_version::board)) &&
         writer.literal(",\"decoder_symbols\":[");
    for (size_t i = 0; ok && i < wxconnect::radio_profile::kEnabledDecoderCount; ++i) {
      const char* symbol = wxconnect::radio_profile::kEnabledDecoderSymbols[i];
      ok = (i == 0 || writer.literal(",")) && writer.string(symbol, std::strlen(symbol));
    }
    if (ok) ok = writer.literal("]");
  }
  if (ok && record.event == Event::Packet)
    ok = writer.literal(",\"native_json\":") &&
         writer.string(record.native_json.data(), record.native_json_size);
  if (ok && record.event == Event::Pulse) {
    ok = writer.literal(",\"pulse_us\":[");
    for (uint32_t i = 0; ok && i < record.captured_count; ++i)
      ok = (i == 0 || writer.literal(",")) && writer.number(record.pulse_us[i]);
    if (ok) ok = writer.literal("],\"gap_us\":[");
    for (uint32_t i = 0; ok && i < record.captured_count; ++i)
      ok = (i == 0 || writer.literal(",")) && writer.number(record.gap_us[i]);
    if (ok) ok = writer.literal("],\"original_count\":") && writer.number(record.original_count) &&
        writer.literal(",\"captured_count\":") && writer.number(record.captured_count) &&
        writer.literal(",\"truncated\":") && writer.literal(record.pulse_truncated ? "true" : "false") &&
        writer.literal(",\"duration_us\":") && writer.number(record.duration_us) &&
        writer.literal(",\"signal_rssi_dbm\":");
    if (ok) ok = record.signal_rssi_valid ? writer.signedNumber(record.signal_rssi_dbm) : writer.literal("null");
    if (ok) ok = writer.literal(",\"packet_seq\":null");
  }
  if (ok && record.event == Event::End && record.capture_diagnostics_valid) {
    const Counters& c = record.capture_counters;
    ok = writer.literal(",\"capture_diagnostics\":{\"decoder_truncation_drops\":") &&
        writer.number(c.decoder_truncation_drops) &&
        writer.literal(",\"queue_full_drops\":") && writer.number(c.queue_full_drops) &&
        writer.literal(",\"oversize_drops\":") && writer.number(c.oversize_drops) &&
        writer.literal(",\"invalid_json_drops\":") && writer.number(c.invalid_json_drops) &&
        writer.literal(",\"storage_drops\":") && writer.number(c.storage_drops) &&
        writer.literal(",\"transport_drops\":") && writer.number(c.transport_drops) &&
        writer.literal(",\"receiver_drops\":") && writer.number(record.receiver_drops) &&
        writer.literal(",\"raw_callback_drops\":") && writer.number(record.raw_callback_drops) &&
        writer.literal(",\"raw_callback_skips\":") && writer.number(record.raw_callback_skips) &&
        writer.literal("},\"stop_reason\":") &&
        writer.string(record.stop_reason.data(), strnlen(record.stop_reason.data(), record.stop_reason.size()));
  }
  if (ok) ok = writer.literal("}\n");
  return ok ? writer.size() : 0;
}
}  // namespace wxconnect::capture
