#pragma once
#include "capture_session.h"
#include <array>
#include <cstring>
namespace wxconnect {
// A bounded latest-packet view, independent of recording and preview selection.
class LivePacketView {
 public:
  bool observe(const char* bytes, size_t length, bool truncated, uint64_t now) {
    if (truncated || length >= json.size() || !capture::detail::JsonValidator(bytes,length).valid()) {
      ++rejected; return false;
    }
    std::memcpy(json.data(),bytes,length); json[length]=0;
    ++received; received_at=now; return true;
  }
  std::array<char,capture::kNativeJsonCapacity> json{};
  uint64_t received=0, rejected=0, received_at=0;
};
class AutomaticCapture {
 public:
  static constexpr uint64_t kMaximumMs=60ULL*60*1000;
  bool claim(bool configured, bool wifi, bool radio, bool blocked, bool active) {
    if (active) attempted_=true; // Do not replace a manually started session.
    if (attempted_ || !configured || !wifi || !radio || blocked) return false;
    attempted_=true; return true;
  }
  bool attempted() const { return attempted_; }
  bool durationExpired(uint64_t now,uint64_t started) const {
    return now>=started && now-started>=kMaximumMs;
  }
 private:
  bool attempted_=false;
};
}
