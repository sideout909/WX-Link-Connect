#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wxconnect::capture::storage_format {
constexpr size_t kHeaderBytes = 12;
constexpr size_t kMaxRecordBytes = 16384;
constexpr uint8_t kMagic[4] = {'W', 'X', 'C', '1'};

inline uint32_t checksumStart() { return 0xffffffffu; }
inline uint32_t checksumUpdate(uint32_t crc, const uint8_t* bytes, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int32_t(crc & 1u)));
  }
  return crc;
}
inline uint32_t checksumFinish(uint32_t crc) { return ~crc; }
inline uint32_t checksum(const uint8_t* bytes, size_t size) {
  return checksumFinish(checksumUpdate(checksumStart(), bytes, size));
}

inline void write32(uint8_t* output, uint32_t value) {
  output[0] = uint8_t(value);
  output[1] = uint8_t(value >> 8);
  output[2] = uint8_t(value >> 16);
  output[3] = uint8_t(value >> 24);
}

inline uint32_t read32(const uint8_t* input) {
  return uint32_t(input[0]) | uint32_t(input[1]) << 8 |
         uint32_t(input[2]) << 16 | uint32_t(input[3]) << 24;
}

inline void header(uint8_t output[kHeaderBytes], const uint8_t* payload, size_t length) {
  std::memcpy(output, kMagic, sizeof(kMagic));
  write32(output + 4, static_cast<uint32_t>(length));
  write32(output + 8, checksum(payload, length));
}

struct Scan {
  size_t valid_bytes = 0;
  uint32_t records = 0;
  bool incomplete_tail = false;
  bool crc_failure = false;
  bool invalid_length = false;
};

inline Scan inspect(const uint8_t* bytes, size_t size) {
  Scan scan;
  while (scan.valid_bytes < size) {
    const size_t remain = size - scan.valid_bytes;
    if (remain < kHeaderBytes) { scan.incomplete_tail = true; break; }
    const uint8_t* frame = bytes + scan.valid_bytes;
    if (std::memcmp(frame, kMagic, sizeof(kMagic))) { scan.invalid_length = true; break; }
    const uint32_t length = read32(frame + 4);
    if (!length || length > kMaxRecordBytes) { scan.invalid_length = true; break; }
    if (remain - kHeaderBytes < length) { scan.incomplete_tail = true; break; }
    if (checksum(frame + kHeaderBytes, length) != read32(frame + 8)) {
      scan.crc_failure = true;
      break;
    }
    scan.valid_bytes += kHeaderBytes + length;
    ++scan.records;
  }
  return scan;
}
}  // namespace wxconnect::capture::storage_format
