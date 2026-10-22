#include "connect_migration_policy.h"
#include "firmware_version.h"
#include <ArduinoJson.h>
#include <fstream>
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
  assert(argc==2);std::ifstream input(argv[1]);assert(input);
  StaticJsonDocument<1536> document;assert(!deserializeJson(document,input));
  using namespace wxconnect::migration_policy;
  Manifest manifest{document["schema"],document["flash_bytes"],document["size"],
      document["board"],document["partition_layout"],document["version"],document["sha256"],document["firmware_url"]};
  assert(validManifest(manifest,8388608,3342336));
  assert(!validManifest(manifest,8388608,1000000));
  manifest.firmware_url="https://example.com/main.bin";
  assert(!validManifest(manifest,8388608,3342336));
  std::cout<<"Public WX-Link manifest accepted for local dev build; small slot and wrong repository rejected\n";
}
