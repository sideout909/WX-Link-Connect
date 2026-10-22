#include "capture_storage_format.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace wxconnect::capture::storage_format;

static void append(std::vector<uint8_t>& file, const char* text) {
  const size_t length = std::strlen(text);
  uint8_t bytes[kHeaderBytes];
  header(bytes, reinterpret_cast<const uint8_t*>(text), length);
  file.insert(file.end(), bytes, bytes + sizeof(bytes));
  file.insert(file.end(), text, text + length);
}

int main() {
  std::vector<uint8_t> file;
  append(file, "{\"event\":\"session\"}\n");
  append(file, "{\"event\":\"packet\",\"seq\":1}\n");
  const Scan whole = inspect(file.data(), file.size());
  assert(whole.records == 2 && whole.valid_bytes == file.size());
  assert(!whole.incomplete_tail && !whole.crc_failure && !whole.invalid_length);

  std::vector<uint8_t> partial = file;
  partial.push_back('W');
  partial.push_back('X');
  const Scan tail = inspect(partial.data(), partial.size());
  assert(tail.records == 2 && tail.valid_bytes == file.size() && tail.incomplete_tail);

  std::vector<uint8_t> corrupt = file;
  corrupt.back() ^= 1;
  const Scan crc = inspect(corrupt.data(), corrupt.size());
  assert(crc.records == 1 && crc.crc_failure && crc.valid_bytes < corrupt.size());

  uint8_t impossible[kHeaderBytes] = {'W', 'X', 'C', '1', 0xff, 0xff, 0xff, 0xff};
  assert(inspect(impossible, sizeof(impossible)).invalid_length);
  std::puts("Capture storage format checks passed: CRC frames, recovery prefix, partial tail and corrupt data.");
}
