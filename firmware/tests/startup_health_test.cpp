#include "startup_health.h"
#include <cassert>
#include <iostream>
int main(){
  using namespace startup_health;
  PortalProbe p;
  assert(!p.due(1800,false));assert(!p.due(7000,false));assert(p.attempts()==0);
  assert(p.due(7001,true));p.result(7200,false);
  assert(!p.due(8699,true));assert(p.due(8700,true));
  p.result(8800,true);assert(p.finished());assert(!p.due(30000,true));
  PortalProbe failed;
  for(unsigned i=0;i<3;++i){assert(failed.due(10000+i*2000,true));failed.result(10000+i*2000,false);}
  assert(failed.finished());assert(!failed.due(90000,true));
  PortalProbe wrapped;wrapped.result(UINT32_MAX-500,false);
  assert(!wrapped.due(998,true));assert(wrapped.due(999,true));
  assert(bootAction(29999,true)==BootAction::Wait);
  assert(bootAction(30000,true)==BootAction::Confirm);
  assert(bootAction(30000,false)==BootAction::Wait);
  assert(bootAction(90000,false)==BootAction::Wait);
  assert(bootAction(90001,false)==BootAction::Rollback);
  assert(bootAction(90001,true)==BootAction::Confirm);
  std::cout<<"Startup health checks passed: HTTP readiness, bounded retries, timer wrap and unchanged confirmation/rollback deadlines.\n";
}
