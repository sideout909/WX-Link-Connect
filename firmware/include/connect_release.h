#pragma once
#include "connect_migration_policy.h"
#include <ArduinoJson.h>

namespace wxconnect::migration_policy {
inline bool connectManifestIdentity(JsonObjectConst document,const char* expected_version,
                                    const char* current_version) {
  return expected_version && document["version"].is<const char*>() &&
      !std::strcmp(document["version"].as<const char*>(),expected_version) &&
      connectUpgrade(expected_version,current_version) &&
      document["product"].is<const char*>() &&
      !std::strcmp(document["product"].as<const char*>(),"WXLink Connect") &&
      document["repository"].is<const char*>() &&
      !std::strcmp(document["repository"].as<const char*>(),kConnectRepository);
}
// GitHub can mark numeric versions as prereleases. Accept those deliberately
// for this experimental product, but never drafts, arbitrary URLs or suffix tags.
inline bool selectConnectRelease(JsonArrayConst releases, char (&output)[18]) {
  bool found=false; std::array<unsigned,3> best{};
  for (JsonObjectConst release : releases) {
    if (!release["draft"].is<bool>() || release["draft"].as<bool>() ||
        !release["prerelease"].is<bool>() || !release["tag_name"].is<const char*>()) continue;
    const char* tag=release["tag_name"];
    std::array<unsigned,3> parts{};
    if (tag[0]!='v' || std::strlen(tag+1)>=sizeof(output) || !version(tag+1,parts)) continue;
    if (!found || parts>best) {
      std::strcpy(output,tag+1); best=parts; found=true;
    }
  }
  return found;
}
}
