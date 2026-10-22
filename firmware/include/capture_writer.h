#pragma once

#include "capture_session.h"

#include <cstddef>

namespace wxconnect::capture {
template <typename Append, typename OnFailure>
bool appendOrStop(CaptureSession& session, Event event, const char* bytes,
                  size_t length, Append append, OnFailure on_failure) {
  if (append(event, bytes, length)) return true;
  session.storageDrop();
  session.abortForStorageFailure();
  on_failure();
  return false;
}
}  // namespace wxconnect::capture
