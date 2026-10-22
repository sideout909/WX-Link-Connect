#pragma once
#include <cstdint>
namespace wxconnect {
// Single loop-task owner. Successful saves get a response window before reboot.
class SetupRestart {
 public:
  bool schedule(uint64_t now, bool busy) {
    if (busy || pending_) return false;
    pending_ = true; deadline_ = now + 2500; return true;
  }
  bool pending() const { return pending_; }
  bool due(uint64_t now) const { return pending_ && now >= deadline_; }
 private:
  bool pending_ = false;
  uint64_t deadline_ = 0;
};
}
