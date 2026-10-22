#pragma once
#include "capture_devices.h"
#include "preview_adapter.h"
namespace wxconnect::preview {
// A const input forces ArduinoJson to copy its strings. Never select the mutable
// zero-copy overload: the same native packet is still needed by other consumers.
inline bool parseNative(const char* bytes, size_t size, JsonDocument& document) {
  return bytes && size && !deserializeJson(document, bytes, size) && document.is<JsonObject>();
}
inline const capture::CandidateIdentity* automaticCandidate(
    const capture::CandidateTable& candidates, const Adapter& adapter) {
  if (adapter.model().selected) return nullptr; // A neighbor must not clear/change selection.
  for (const auto& candidate : candidates.entries())
    if (candidate.occupied && candidate.has_model && candidate.has_id &&
        candidate.packets >= 3 && Adapter::supportsModel(candidate.model.data()))
      return &candidate;
  return nullptr;
}
}
