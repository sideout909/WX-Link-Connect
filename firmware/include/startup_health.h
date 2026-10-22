#pragma once
#include <cstdint>

namespace startup_health {
// A timer alone does not establish that the main task can answer HTTP yet.
class PortalProbe {
 public:
  bool due(uint32_t now,bool serving)const {
    return serving&&!passed_&&attempts_<3&&(!attempts_||int32_t(now-next_)>=0);
  }
  void result(uint32_t now,bool passed){++attempts_;passed_=passed;next_=now+1500;}
  bool finished()const{return passed_||attempts_>=3;}
  unsigned attempts()const{return attempts_;}
 private:
  uint32_t next_=0;
  unsigned attempts_=0;
  bool passed_=false;
};
enum class BootAction { Wait,Confirm,Rollback };
inline BootAction bootAction(uint32_t elapsed,bool healthy){
  if(elapsed<30000)return BootAction::Wait;
  if(healthy)return BootAction::Confirm;
  return elapsed>90000?BootAction::Rollback:BootAction::Wait;
}
}
