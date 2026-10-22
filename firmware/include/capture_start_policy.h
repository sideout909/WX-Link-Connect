#pragma once
#include <cstddef>
#include <cstdint>
namespace wxconnect::capture {
inline const char* storageStartBlocker(bool ready, bool active, size_t retained,
    size_t limit, uint64_t free_bytes, uint64_t minimum_free) {
  if (!ready) return "Capture storage is unavailable; check storage status. No data was erased.";
  if (active) return "Recording is already running.";
  if (retained >= limit) return "Eight sessions retained. Export and delete a Connect session to start another.";
  if (free_bytes < minimum_free) return "Storage reserve reached. Export and delete Connect sessions to free space.";
  return nullptr;
}
}
