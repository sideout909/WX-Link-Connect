#pragma once

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

namespace wxconnect::migration_policy {

constexpr const char* kRepository = "sideout909/WX-Link";
constexpr const char* kBoard = "panellan_sc05_x";
constexpr const char* kPartitionLayout = "default_8MB-v1";
constexpr const char* kManifestUrl =
    "https://github.com/sideout909/WX-Link/releases/latest/download/"
    "weather-link-panellan_sc05_x.json";
constexpr const char* kFirmwareName = "weather-link-panellan_sc05_x.bin";

struct Manifest {
  uint32_t schema = 0;
  uint32_t flash_bytes = 0;
  uint32_t size = 0;
  const char* board = nullptr;
  const char* partition_layout = nullptr;
  const char* version = nullptr;
  const char* sha256 = nullptr;
  const char* firmware_url = nullptr;
};

inline bool version(const char* value, std::array<unsigned, 3>& parts) {
  if (!value || !*value) return false;
  size_t at = 0;
  for (unsigned i = 0; i < parts.size(); ++i) {
    unsigned number = 0, digits = 0;
    while (value[at] >= '0' && value[at] <= '9') {
      number = number * 10 + unsigned(value[at++] - '0');
      if (++digits > 5 || number > 65535) return false;
    }
    if (!digits) return false;
    parts[i] = number;
    if (i + 1 < parts.size()) {
      if (value[at++] != '.') return false;
    }
  }
  return value[at] == '\0';
}

inline bool sha256(const char* value) {
  if (!value || std::strlen(value) != 64) return false;
  const auto* cursor = reinterpret_cast<const unsigned char*>(value);
  for (; *cursor; ++cursor) {
    if (!std::isxdigit(*cursor)) return false;
  }
  return true;
}

inline bool firmwareUrl(const char* value, const char* version_text) {
  if (!value || !version_text || std::strlen(value) >= 1024) return false;
  std::array<unsigned, 3> parts{};
  if (!version(version_text, parts)) return false;
  char expected[192];
  const int length = std::snprintf(expected, sizeof(expected),
      "https://github.com/%s/releases/download/v%s/%s",
      kRepository, version_text, kFirmwareName);
  return length > 0 && size_t(length) < sizeof(expected) &&
         std::strcmp(value, expected) == 0;
}

inline bool downloadHost(const char* value) {
  if (!value || std::strpbrk(value, "\r\n") || std::strlen(value) >= 2048) return false;
  for (const char* prefix : {
           "https://github.com/",
           "https://release-assets.githubusercontent.com/",
           "https://objects.githubusercontent.com/",
           "https://github-releases.githubusercontent.com/"}) {
    if (std::strncmp(value, prefix, std::strlen(prefix)) == 0) return true;
  }
  return false;
}

inline bool validManifest(const Manifest& manifest,
                          uint32_t expected_flash_bytes, uint32_t update_slot_bytes) {
  std::array<unsigned, 3> target_version{};
  // Connect and WX-Link are different products. Their release numbers cannot
  // be compared as an in-product upgrade/downgrade check. Only stable, numeric
  // main releases at the fixed main-repository URL are accepted.
  return manifest.schema == 1 && manifest.board &&
         std::strcmp(manifest.board, kBoard) == 0 && manifest.partition_layout &&
         std::strcmp(manifest.partition_layout, kPartitionLayout) == 0 &&
         manifest.flash_bytes == expected_flash_bytes && manifest.size >= 32768 &&
         manifest.size <= update_slot_bytes && version(manifest.version, target_version) &&
         sha256(manifest.sha256) && firmwareUrl(manifest.firmware_url, manifest.version);
}

}  // namespace wxconnect::migration_policy
