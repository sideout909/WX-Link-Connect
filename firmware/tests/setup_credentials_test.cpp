#include "setup_credentials.h"
#include <cassert>
#include <string>
#include <iostream>
int main(){
  unsigned generated=0;auto random=[&]{return std::string("GENERATED-")+std::to_string(++generated);};
  std::string wifi,admin;bool custom=false;
  assert(setup_credentials::ensure(wifi,admin,custom,random));
  assert(wifi==admin&&!custom);const auto original_wifi=wifi;
  setup_credentials::chooseAdmin(wifi,admin,custom,std::string("OWNER-PASSWORD"),random);
  assert(admin=="OWNER-PASSWORD"&&custom&&wifi==original_wifi&&wifi!=admin);
  setup_credentials::chooseAdmin(wifi,admin,custom,std::string(""),random);
  assert(admin=="OWNER-PASSWORD"&&custom&&wifi==original_wifi);
  assert(!setup_credentials::ensure(wifi,admin,custom,random));
  const auto displayed=wifi;setup_credentials::chooseAdmin(wifi,admin,custom,displayed,random);
  assert(custom&&admin==displayed&&wifi!=admin);
  wifi.clear();admin="LEGACY-CUSTOM-PASSWORD";custom=false;
  assert(setup_credentials::ensure(wifi,admin,custom,random));
  assert(custom&&admin=="LEGACY-CUSTOM-PASSWORD"&&wifi!=admin);
  assert(!setup_credentials::ensure(wifi,admin,custom,random));
  std::cout<<"Setup credential checks passed: bootstrap, custom password privacy, blank retention, reboot, legacy migration and AP-key collision.\n";
}
