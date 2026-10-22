#include "weather_store.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

using namespace weather;
Sample at(uint64_t t, uint8_t type = 37) { Sample s; s.time_ms = t; s.type = type; return s; }

int main() {
  std::vector<Bucket> history(kHistoryBuckets);
  Store model(history.data());
  auto a = at(1000000); a.wind.valid = true; a.wind.value = 30;
  a.temperature.valid = true; a.temperature.value = 55;
  assert(model.ingest(a) == Result::Accepted);
  auto duplicate = a; duplicate.time_ms += 400;
  assert(model.ingest(duplicate) == Result::Duplicate);
  assert(model.wind.time_ms == a.time_ms); // Repeats cannot freshen peaks.
  auto b = at(1010000, 39); b.wind.valid = true; b.wind.value = 10;
  b.lux.valid = true; b.lux.value = 0;
  assert(model.ingest(b) == Result::Accepted);
  assert(model.temperature.time_ms == a.time_ms); // Sparse packet ages are independent.
  assert(model.lux.valid && model.lux.value == 0);
  assert(model.gust(1299999).value == 30);
  assert(model.gust(1300000).value == 10); // Exact left-boundary expiry.
  assert(!model.gust(1310000).valid);
  assert(!model.temperature.fresh(1120000));
  auto wrong = at(1320000); wrong.station = 1;
  assert(model.ingest(wrong) == Result::WrongStation);
  wrong.station = 679; wrong.exception = true;
  assert(model.ingest(wrong) == Result::Flagged);
  auto invalid = at(1330000); invalid.wind.valid = true;
  invalid.wind.value = std::numeric_limits<float>::quiet_NaN();
  assert(model.ingest(invalid) == Result::Invalid);
  assert(model.ingest(at(500000)) == Result::Backwards);

  Store rain;
  auto r = at(1000000,38); r.rain_tips.valid = true; r.rain_tips.value = 510;
  assert(rain.ingest(r) == Result::Accepted);
  assert(rain.session_rain_tips == 0); // Initial counter is a baseline.
  r.time_ms += 30000; r.rain_tips.value = 2;
  assert(rain.ingest(r) == Result::Accepted);
  assert(rain.session_rain_tips == 4 && rain.rain_incomplete);
  r.time_ms += 30000; r.rain_tips.value = 7; rain.ingest(r);
  assert(rain.session_rain_tips == 9);
  r.time_ms += 30000; r.rain_tips.value = 0; rain.ingest(r);
  assert(rain.session_rain_tips == 9 && rain.rain_discontinuities == 2);
  r.time_ms += 30000; r.rain_tips.value = 1; rain.ingest(r);
  assert(rain.session_rain_tips == 10); // Rebased after unexpected reset.
  auto z = at(2000000); z.wind.valid = true; z.wind.value = 0;
  rain.ingest(z); assert(rain.gust(z.time_ms).valid && rain.gust(z.time_ms).value == 0);

  Store coverage;
  for (uint64_t t=1000000;t<=1300000;t+=10000) {
    auto s=at(t); s.wind.valid=true; s.wind.value=5; coverage.ingest(s);
  }
  assert(coverage.gustComplete(1300000));
  auto gap=at(1400000); gap.wind.valid=true; gap.wind.value=4; coverage.ingest(gap);
  assert(!coverage.gustComplete(1400000));
  assert(model.bucket(1000000/60000)->temperature_count == 1);
  auto replace=at(1000000+1440ULL*60000); replace.temperature.valid=true;replace.temperature.value=60;
  model.ingest(replace);
  assert(model.bucket(1000000/60000) == nullptr); // Ring overwrite doesn't expose old data.
  assert(model.bucket(replace.time_ms/60000)->temperature_count == 1);
  // Checkpoint restore copies buckets into a new Store without accepting live
  // packets. History must remain available without making old fields fresh.
  std::vector<Bucket> saved_history = history;
  Store restored(saved_history.data());
  const auto restored_time = replace.time_ms;
  assert(restored.accepted == 0 && !restored.temperature.valid);
  auto summary = restored.historySummary(restored_time);
  assert(summary.buckets == 1 && summary.first_minute == restored_time / 60000);
  assert(summary.last_minute == summary.first_minute);
  assert(restored.bucket(summary.first_minute)->temperature_sum == 60);
  assert(restored.historySummary(restored_time + 1440ULL * 60000).buckets == 0);
  assert(restored.historySummary(restored_time - 60000).buckets == 0);
  Store empty;
  assert(empty.historySummary(restored_time).buckets == 0);
  std::cout << "Weather store checks passed: sparse ages, duplicate timing, gust expiry/coverage, zeros, binding, invalid data, rain reset/wrap and history overwrite.\n";
}
