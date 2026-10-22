#include "capture_storage.h"

#include "capture_storage_format.h"
#include "capture_start_policy.h"

#include <LittleFS.h>
#include <esp_timer.h>
#include <algorithm>
#include <cstring>

namespace wxconnect::capture::storage {
namespace {
using storage_format::kHeaderBytes;
using storage_format::kMaxRecordBytes;
constexpr const char* kDirectory = "/connect/captures";
constexpr size_t kBatchCapacity = kMaxRecordBytes + kHeaderBytes;
constexpr uint64_t kMinimumFreeToStart = kFilesystemReserveBytes + 4096;
File files[2];
std::array<SessionInfo, kSessionLimit> session_list{};
uint8_t current_session = UINT8_MAX;
uint8_t batch_kind = UINT8_MAX;
uint8_t batch[kBatchCapacity];
size_t batch_size = 0;
uint32_t batch_records = 0;
uint64_t last_flush_ms = 0;
uint32_t storage_drops = 0, crc_failures = 0;
bool initialized = false;
bool mounted = false, session_active = false, write_failed = false;
String message = "Capture storage not initialized";

String pathFor(const char* id, bool pulses) {
  return String(kDirectory) + "/" + id + (pulses ? ".pulses.wxc" : ".decoded.wxc");
}

bool validId(const char* id) {
  if (!id || std::strlen(id) != kSessionIdChars) return false;
  for (size_t i = 0; i < kSessionIdChars; ++i)
    if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
  return true;
}

int findSession(const char* id) {
  for (size_t i = 0; i < session_list.size(); ++i)
    if (session_list[i].occupied && std::strcmp(session_list[i].id.data(), id) == 0)
      return static_cast<int>(i);
  return -1;
}

int freeSession() {
  for (size_t i = 0; i < session_list.size(); ++i)
    if (!session_list[i].occupied) return static_cast<int>(i);
  return -1;
}

uint32_t scanFile(const String& path, bool pulses, bool& end_seen,
                  bool& corrupt, bool& incomplete, uint32_t& records) {
  File file = LittleFS.open(path, "r");
  if (!file) return 0;
  const size_t total = file.size();
  size_t offset = 0;
  uint8_t header[kHeaderBytes], chunk[256], prefix[97];
  while (offset < total) {
    if (total - offset < kHeaderBytes || file.read(header, sizeof(header)) != sizeof(header)) {
      incomplete = true;
      break;
    }
    if (std::memcmp(header, storage_format::kMagic, sizeof(storage_format::kMagic))) {
      corrupt = true;
      break;
    }
    const uint32_t length = storage_format::read32(header + 4);
    if (!length || length > kMaxRecordBytes) { corrupt = true; break; }
    if (total - offset - kHeaderBytes < length) { incomplete = true; break; }
    uint32_t crc = storage_format::checksumStart();
    size_t remaining = length;
    const size_t prefix_size = std::min<size_t>(length, sizeof(prefix) - 1);
    if (file.read(prefix, prefix_size) != prefix_size) { incomplete = true; break; }
    prefix[prefix_size] = 0;
    crc = storage_format::checksumUpdate(crc, prefix, prefix_size);
    remaining -= prefix_size;
    while (remaining) {
      const size_t count = std::min(remaining, sizeof(chunk));
      if (file.read(chunk, count) != count) { incomplete = true; break; }
      crc = storage_format::checksumUpdate(crc, chunk, count);
      remaining -= count;
    }
    if (incomplete) break;
    if (storage_format::checksumFinish(crc) != storage_format::read32(header + 8)) {
      corrupt = true;
      ++crc_failures;
      break;
    }
    if (!pulses && std::strstr(reinterpret_cast<const char*>(prefix), "\"event\":\"end\""))
      end_seen = true;
    offset += kHeaderBytes + length;
    ++records;
  }
  file.close();
  return static_cast<uint32_t>(offset);
}

bool recover() {
  session_list = {};
  File directory = LittleFS.open(kDirectory);
  if (!directory || !directory.isDirectory()) { message = "Capture directory unavailable"; return false; }
  while (File entry = directory.openNextFile()) {
    if (entry.isDirectory()) { entry.close(); continue; }
    String name = entry.name();
    const bool pulses = name.endsWith(".pulses.wxc");
    if (!pulses && !name.endsWith(".decoded.wxc")) { entry.close(); continue; }
    int slash = name.lastIndexOf('/');
    const String base = slash >= 0 ? name.substring(slash + 1) : name;
    if (base.length() < kSessionIdChars + 12) { entry.close(); continue; }
    const String id = base.substring(0, kSessionIdChars);
    if (!validId(id.c_str())) { entry.close(); continue; }
    int index = findSession(id.c_str());
    if (index < 0) {
      index = freeSession();
      if (index < 0) { entry.close(); continue; }
      SessionInfo& created = session_list[index];
      std::memcpy(created.id.data(), id.c_str(), kSessionIdChars + 1);
      created.occupied = true;
    }
    entry.close();
    SessionInfo& info = session_list[index];
    bool end_seen = false;
    uint32_t records = 0;
    const String full_path = pathFor(info.id.data(), pulses);
    const uint32_t valid_bytes = scanFile(full_path, pulses, end_seen, info.corrupt,
                                          info.incomplete_tail, records);
    if (pulses) { info.pulse_valid_bytes = valid_bytes; info.pulse_records = records; }
    else { info.decoded_valid_bytes = valid_bytes; info.decoded_records = records; }
    if (!pulses) info.interrupted = !end_seen;
  }
  directory.close();
  message = "Capture storage ready; existing sessions preserved";
  return true;
}

bool flushBatch() {
  if (write_failed) return false;
  if (!batch_size) return true;
  if (!session_active || current_session >= session_list.size() || batch_kind > 1 || write_failed)
    return false;
  const uint64_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  if (total <= used || total - used < kFilesystemReserveBytes + batch_size) {
    ++storage_drops;
    message = "Capture stopped at storage reserve; saved files preserved";
    write_failed = true;
    batch_size = 0;
    return false;
  }
  File& file = files[batch_kind];
  const size_t old_size = file.size();
  if (!file || old_size + batch_size > kPoolBytes ||
      file.write(batch, batch_size) != batch_size) {
    ++storage_drops;
    message = "Capture storage write failed; partial tail will be recovered";
    write_failed = true;
    batch_size = 0;
    return false;
  }
  file.flush();
  SessionInfo& info = session_list[current_session];
  if (batch_kind == 0) {
    info.decoded_bytes += batch_size;
    info.decoded_records += batch_records;
    info.decoded_valid_bytes = info.decoded_bytes;
  } else {
    info.pulse_bytes += batch_size;
    info.pulse_records += batch_records;
    info.pulse_valid_bytes = info.pulse_bytes;
  }
  batch_size = 0;
  batch_records = 0;
  last_flush_ms = uint64_t(esp_timer_get_time()) / 1000;
  return true;
}
}  // namespace

bool begin() {
  if (initialized) return true;
  if (!mounted) mounted = LittleFS.begin(false);
  if (!mounted) { message = "LittleFS mount failed; no format attempted"; return false; }
  if (!LittleFS.exists("/connect") && !LittleFS.mkdir("/connect")) {
    message = "Connect storage directory unavailable";
    return false;
  }
  if (!LittleFS.exists(kDirectory) && !LittleFS.mkdir(kDirectory)) {
    message = "Capture directory unavailable";
    return false;
  }
  if (!recover()) return false;
  initialized = true;
  return true;
}

bool beginSession(const char* session_id) {
  if (!initialized && !begin()) return false;
  if (const char* blocker = state().start_blocker) { message = blocker; return false; }
  if (session_active || !validId(session_id) || freeSession() < 0 || findSession(session_id) >= 0) {
    message = "Session limit reached or session identifier already exists";
    return false;
  }
  const uint64_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  if (total <= used || total - used < kMinimumFreeToStart) {
    message = "Capture refused: less than 512 KiB reserve remains";
    return false;
  }
  const String decoded_path = pathFor(session_id, false), pulse_path = pathFor(session_id, true);
  if (LittleFS.exists(decoded_path) || LittleFS.exists(pulse_path)) {
    message = "Session files already exist; refusing overwrite";
    return false;
  }
  int index = freeSession();
  if (index < 0) return false;
  files[0] = LittleFS.open(decoded_path, "w");
  files[1] = LittleFS.open(pulse_path, "w");
  if (!files[0] || !files[1]) {
    files[0].close(); files[1].close();
    LittleFS.remove(decoded_path); LittleFS.remove(pulse_path);
    message = "Capture session files could not be created";
    return false;
  }
  SessionInfo& info = session_list[index];
  info = SessionInfo{};
  std::memcpy(info.id.data(), session_id, kSessionIdChars + 1);
  info.occupied = true;
  current_session = static_cast<uint8_t>(index);
  batch_size = batch_records = 0;
  batch_kind = UINT8_MAX;
  storage_drops = 0;
  write_failed = false;
  session_active = true;
  last_flush_ms = uint64_t(esp_timer_get_time()) / 1000;
  message = "Capture session recording to bounded local storage";
  return true;
}

bool append(Event event, const char* jsonl, size_t length) {
  if (!session_active || write_failed || !jsonl || !length || length > kMaxRecordBytes) return false;
  const uint8_t kind = event == Event::Pulse ? 1 : 0;
  if (batch_kind != UINT8_MAX && batch_kind != kind && !flushBatch()) return false;
  batch_kind = kind;
  const size_t frame_size = kHeaderBytes + length;
  if (batch_size + frame_size > sizeof(batch) && !flushBatch()) return false;
  if (batch_size + frame_size > sizeof(batch)) { ++storage_drops; return false; }
  const uint32_t pool_size = kind == 0 ? session_list[current_session].decoded_bytes
                                       : session_list[current_session].pulse_bytes;
  if (uint64_t(pool_size) + batch_size + frame_size > kPoolBytes) {
    ++storage_drops;
    message = kind == 0 ? "Decoded storage budget reached" : "Raw pulse storage budget reached";
    // Persist the already accepted prefix before reporting the terminal limit.
    if (!flushBatch()) return false;
    write_failed = true;
    return false;
  }
  uint8_t* frame_header = batch + batch_size;
  storage_format::header(frame_header, reinterpret_cast<const uint8_t*>(jsonl), length);
  std::memcpy(frame_header + kHeaderBytes, jsonl, length);
  batch_size += frame_size;
  ++batch_records;
  return true;
}

bool flush() { return flushBatch(); }

bool tick() {
  if (!session_active || !batch_size) return true;
  const uint64_t now = uint64_t(esp_timer_get_time()) / 1000;
  if (now - last_flush_ms < 2000) return true;
  return flushBatch();
}

bool finish() {
  if (!session_active) return false;
  const bool ok = flushBatch();
  files[0].close(); files[1].close();
  session_active = false;
  current_session = UINT8_MAX;
  batch_kind = UINT8_MAX;
  message = ok ? "Capture session saved" : "Capture ended with storage loss; valid prefix preserved";
  return ok;
}

bool deleteSession(const char* session_id) {
  if (!mounted || !validId(session_id)) return false;
  const int index = findSession(session_id);
  if (index < 0 || (session_active && current_session == index)) return false;
  const String decoded_path = pathFor(session_id, false);
  const String pulse_path = pathFor(session_id, true);
  bool ok = true;
  if (LittleFS.exists(decoded_path) && !LittleFS.remove(decoded_path)) ok = false;
  if (LittleFS.exists(pulse_path) && !LittleFS.remove(pulse_path)) ok = false;
  if (LittleFS.exists(decoded_path) || LittleFS.exists(pulse_path)) {
    if (!recover()) initialized = false;
    message = "Session deletion incomplete; remaining evidence was retained";
    return false;
  }
  session_list[index] = SessionInfo{};
  message = ok ? "Session deleted and retained storage slot released"
               : "Session files were already absent; retained storage slot released";
  return true;
}

bool readRecord(const char* session_id, bool pulses, uint32_t offset,
                char* output, size_t capacity, size_t& length, uint32_t& next_offset) {
  if (!mounted || !validId(session_id) || !output) return false;
  const int index = findSession(session_id);
  if (index < 0) return false;
  const SessionInfo& info = session_list[index];
  const uint32_t valid_bytes = pulses ? info.pulse_valid_bytes : info.decoded_valid_bytes;
  if (offset >= valid_bytes) return false;
  File file = LittleFS.open(pathFor(session_id, pulses), "r");
  if (!file || !file.seek(offset)) return false;
  uint8_t header[kHeaderBytes];
  if (file.read(header, sizeof(header)) != sizeof(header) ||
      std::memcmp(header, storage_format::kMagic, sizeof(storage_format::kMagic))) return false;
  length = storage_format::read32(header + 4);
  if (!length || length > capacity || length > kMaxRecordBytes) return false;
  if (file.read(reinterpret_cast<uint8_t*>(output), length) != length) return false;
  if (storage_format::checksum(reinterpret_cast<const uint8_t*>(output), length) !=
      storage_format::read32(header + 8)) return false;
  next_offset = offset + kHeaderBytes + length;
  return true;
}

const std::array<SessionInfo, kSessionLimit>& sessions() { return session_list; }

State state() {
  State result;
  result.ready = initialized;
  result.active = session_active;
  result.total_bytes = LittleFS.totalBytes();
  result.used_bytes = LittleFS.usedBytes();
  result.free_bytes = result.total_bytes > result.used_bytes ? result.total_bytes - result.used_bytes : 0;
  result.storage_drops = storage_drops;
  result.crc_failures = crc_failures;
  for (const auto& session : session_list) if (session.occupied) {
    ++result.sessions;
    if (session.interrupted) ++result.recovered_sessions;
  }
  result.start_blocker = storageStartBlocker(result.ready, result.active, result.sessions,
      kSessionLimit, result.free_bytes, kMinimumFreeToStart);
  return result;
}
const char* statusMessage() { return message.c_str(); }
}  // namespace wxconnect::capture::storage
