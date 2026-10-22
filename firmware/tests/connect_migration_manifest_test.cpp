#include "connect_migration_policy.h"
#include "firmware_version.h"
#include "connect_release.h"
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
  StaticJsonDocument<2048> releases;
  assert(!deserializeJson(releases,R"JSON([
    {"draft":false,"prerelease":true,"tag_name":"v0.1.1"},
    {"draft":true,"prerelease":false,"tag_name":"v9.9.9"},
    {"draft":false,"prerelease":true,"tag_name":"v0.1.7-beta"},
    {"draft":false,"prerelease":false,"tag_name":"v0.1.6"},
    {"draft":false,"prerelease":true,"tag_name":"v0.1.5"}
  ])JSON"));
  char latest[18]{};
  assert(selectConnectRelease(releases.as<JsonArrayConst>(),latest));
  assert(!std::strcmp(latest,"0.1.6"));
  assert(connectUpgrade(latest,firmware_version::current));
  releases.clear();
  assert(!deserializeJson(releases,R"JSON([{"tag_name":"v99.0.0"},{"draft":false,"prerelease":false,"tag_name":"../evil"}])JSON"));
  assert(!selectConnectRelease(releases.as<JsonArrayConst>(),latest));
  std::cout<<"Connect release selection: numeric prereleases, drafts, hostile tags and downgrade guards passed\n";

  document.clear();
  document["version"]="0.1.6";document["product"]="WXLink Connect";
  document["repository"]=kConnectRepository;
  assert(connectManifestIdentity(document.as<JsonObjectConst>(),"0.1.6","0.1.5-dev"));
  assert(!connectManifestIdentity(document.as<JsonObjectConst>(),"0.1.7","0.1.5-dev"));
  assert(!connectManifestIdentity(document.as<JsonObjectConst>(),"0.1.6","0.1.7"));
  document["product"]="WX-Link";
  assert(!connectManifestIdentity(document.as<JsonObjectConst>(),"0.1.6","0.1.5-dev"));
  document["product"]="WXLink Connect";document["repository"]="attacker/WX-Link-Connect";
  assert(!connectManifestIdentity(document.as<JsonObjectConst>(),"0.1.6","0.1.5-dev"));

}
