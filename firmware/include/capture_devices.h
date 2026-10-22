#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wxconnect::capture {
constexpr size_t kCandidateCapacity = 32;
constexpr size_t kIdentityFieldCapacity = 40;

struct CandidateIdentity {
  std::array<char, 128> protocol{};
  std::array<char, kIdentityFieldCapacity> model{};
  std::array<char, kIdentityFieldCapacity> id{};
  std::array<char, kIdentityFieldCapacity> channel{};
  bool has_protocol = false, has_model = false, has_id = false, has_channel = false;
  uint32_t packets = 0;
  uint64_t last_uptime_ms = 0;
  bool occupied = false;

  bool complete() const { return has_protocol && has_model && has_id; }
};

class CandidateTable {
 public:
  bool observe(const char* protocol, const char* model, const char* id,
               const char* channel, uint64_t uptime_ms) {
    CandidateIdentity key;
    if (!copy(protocol, key.protocol, key.has_protocol) ||
        !copy(model, key.model, key.has_model) ||
        !copy(id, key.id, key.has_id) ||
        !copy(channel, key.channel, key.has_channel)) {
      ++oversize_;
      return false;
    }
    CandidateIdentity* slot = nullptr;
    for (auto& entry : entries_) {
      if (entry.occupied && same(entry, key)) {
        slot = &entry;
        break;
      }
    }
    if (!slot) {
      for (auto& entry : entries_) {
        if (!entry.occupied) {
          slot = &entry;
          break;
        }
      }
    }
    if (!slot) {
      ++overflow_;
      return false;
    }
    if (!slot->occupied) {
      *slot = key;
      slot->occupied = true;
    }
    if (slot->packets != UINT32_MAX) ++slot->packets;
    slot->last_uptime_ms = uptime_ms;
    return true;
  }

  const std::array<CandidateIdentity, kCandidateCapacity>& entries() const { return entries_; }
  uint32_t overflow() const { return overflow_; }
  uint32_t oversize() const { return oversize_; }

 private:
  template <size_t N>
  static bool copy(const char* source, std::array<char, N>& destination, bool& present) {
    destination.fill('\0');
    present = source && source[0];
    if (!present) return true;
    const size_t length = std::strlen(source);
    if (length >= N) return false;
    std::memcpy(destination.data(), source, length);
    return true;
  }

  template <size_t N>
  static bool equal(const std::array<char, N>& a, bool a_present,
                    const std::array<char, N>& b, bool b_present) {
    return a_present == b_present && (!a_present || std::strcmp(a.data(), b.data()) == 0);
  }

  static bool same(const CandidateIdentity& a, const CandidateIdentity& b) {
    return equal(a.protocol, a.has_protocol, b.protocol, b.has_protocol) &&
           equal(a.model, a.has_model, b.model, b.has_model) &&
           equal(a.id, a.has_id, b.id, b.has_id) &&
           equal(a.channel, a.has_channel, b.channel, b.has_channel);
  }

  std::array<CandidateIdentity, kCandidateCapacity> entries_{};
  uint32_t overflow_ = 0, oversize_ = 0;
};
}  // namespace wxconnect::capture
