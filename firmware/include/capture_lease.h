#pragma once

#include <cstdint>

namespace wxconnect::capture {
class CaptureLease {
 public:
  enum class Owner : uint8_t { None, Usb, Browser };
  static constexpr uint64_t kTimeoutMs = 20000;

  bool begin(Owner owner, uint64_t now_ms) {
    if (owner == Owner::None || owner_ != Owner::None) return false;
    owner_ = owner;
    last_heartbeat_ms_ = now_ms;
    return true;
  }

  bool heartbeat(Owner owner, uint64_t now_ms) {
    if (owner == Owner::None || owner_ != owner) return false;
    last_heartbeat_ms_ = now_ms;
    return true;
  }

  bool expired(uint64_t now_ms) const {
    return owner_ != Owner::None && now_ms >= last_heartbeat_ms_ &&
           now_ms - last_heartbeat_ms_ > kTimeoutMs;
  }

  Owner owner() const { return owner_; }
  void clear() { owner_ = Owner::None; last_heartbeat_ms_ = 0; }

 private:
  Owner owner_ = Owner::None;
  uint64_t last_heartbeat_ms_ = 0;
};
}  // namespace wxconnect::capture
