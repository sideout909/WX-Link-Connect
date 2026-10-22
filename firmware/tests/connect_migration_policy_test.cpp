#include "connect_migration_policy.h"

#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  using namespace wxconnect::migration_policy;
  std::array<unsigned, 3> parts{};
  assert(version("0.7.5", parts) && parts[0] == 0 && parts[1] == 7 && parts[2] == 5);
  assert(!version("0.7.5-beta", parts));
  assert(!version("0.7", parts));
  assert(!version("0.70000.0", parts));
  assert(!version("0.8.0-beta", parts));
  assert(!version("0.1.4-dev", parts)); // Target is stable; Connect's own version is independent.


  char url[192];
  const int size = std::snprintf(url, sizeof(url),
      "https://github.com/%s/releases/download/v0.7.5/%s", kRepository, kFirmwareName);
  assert(size > 0 && size_t(size) < sizeof(url));
  const char digest[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  Manifest manifest{1, 8388608, 1100000, kBoard, kPartitionLayout,
                    "0.7.5", digest, url};
  assert(validManifest(manifest, 8388608, 3342336));

  manifest.board = "panellan_sc01_plus";
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.board = kBoard;
  manifest.partition_layout = "other-partitions";
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.partition_layout = kPartitionLayout;
  manifest.flash_bytes = 16777216;
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.flash_bytes = 8388608;
  manifest.size = 3342337;
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.size = 1100000;
  manifest.version = "0.1.0";
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.version = "0.7.5";
  manifest.firmware_url = "https://github.com/attacker/WX-Link/releases/download/v0.7.5/weather-link-panellan_sc05_x.bin";
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.firmware_url = url;
  manifest.sha256 = "not-a-digest";
  assert(!validManifest(manifest, 8388608, 3342336));
  manifest.sha256 = digest;
  assert(validManifest(manifest, 8388608, 3342336));

  assert(downloadHost("https://github.com/sideout909/WX-Link/releases/download/v0.7.5/image.bin"));
  assert(downloadHost("https://release-assets.githubusercontent.com/signed/image.bin"));
  assert(!downloadHost("http://github.com/sideout909/WX-Link/image.bin"));
  assert(!downloadHost("https://github.com.attacker.example/image.bin"));
  assert(!downloadHost("https://objects.githubusercontent.com/image.bin\r\nHost: evil"));
}
