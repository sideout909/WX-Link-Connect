#pragma once

#include <array>
#include "rolling_rain.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace weather {

constexpr uint64_t kGustWindowMs = 300000;
constexpr uint64_t kStaleMs = 120000;
constexpr size_t kHistoryBuckets = 1440;

template <typename T> struct Field {
  T value{};
  uint64_t time_ms = 0;
  bool valid = false;
  void set(T next, uint64_t time) { value = next; time_ms = time; valid = true; }
  bool fresh(uint64_t now, uint64_t max_age = kStaleMs) const {
    return valid && now >= time_ms && now - time_ms < max_age;
  }
};

struct Sample {
  uint64_t time_ms = 0;
  uint16_t station = 679;
  char channel = 'A';
  uint8_t type = 0;
  bool exception = false;
  // Missing fields stay absent. The serial adapter sets validity explicitly.
  Field<float> temperature, wind, direction, uv;
  Field<uint32_t> lux;
  Field<uint16_t> humidity, rain_tips, strikes;
};

struct Bucket {
  uint64_t minute = UINT64_MAX;
  float temperature_sum = 0;
  float wind_max = 0;
  uint32_t lux_sum = 0;
  uint32_t humidity_sum = 0;
  uint16_t temperature_count = 0, wind_count = 0, lux_count = 0, humidity_count = 0;
  uint16_t rain_tips = 0;
  bool rain_observed = false;
};

enum class Result { Accepted, Duplicate, WrongStation, Flagged, Invalid, Backwards };

class Store {
 public:
  explicit Store(Bucket* history = nullptr, uint16_t station = 679, char channel = 'A', RainInterval* rain_history = nullptr) : rain_timeline(rain_history), history_(history), station_(station), channel_(channel) {}
  RollingRain rain_timeline;
  Field<float> temperature, wind, direction, uv;
  Field<uint32_t> lux;
  Field<uint16_t> humidity, strikes;
  uint64_t latest_ms = 0;
  uint32_t accepted = 0, duplicates = 0, rejected = 0;
  uint32_t session_rain_tips = 0;
  uint32_t rain_discontinuities = 0;
  bool rain_baseline = false, rain_incomplete = true;
  bool wind_capacity_lost = false;

  Result ingest(const Sample& s) {
    if (s.station != station_ || s.channel != channel_) { ++rejected; return Result::WrongStation; }
    if (s.exception) { ++rejected; return Result::Flagged; }
    if (!s.time_ms || !valid(s)) { ++rejected; return Result::Invalid; }
    if (accepted && s.time_ms < latest_ms) { ++rejected; return Result::Backwards; }
    if (accepted && s.time_ms - last_.time_ms < 3000 && samePayload(s, last_)) {
      ++duplicates; return Result::Duplicate;
    }
    last_ = s; latest_ms = s.time_ms; ++accepted;
    if (s.temperature.valid) temperature.set(s.temperature.value, s.time_ms);
    if (s.humidity.valid) humidity.set(s.humidity.value, s.time_ms);
    if (s.direction.valid) direction.set(s.direction.value, s.time_ms);
    if (s.lux.valid) lux.set(s.lux.value, s.time_ms);
    if (s.uv.valid) uv.set(s.uv.value, s.time_ms);
    if (s.strikes.valid) strikes.set(s.strikes.value, s.time_ms);
    if (s.wind.valid) {
      wind.set(s.wind.value, s.time_ms);
      prune(s.time_ms);
      if (wind_size_ == wind_samples_.size()) {
        wind_head_ = (wind_head_ + 1) % wind_samples_.size();
        --wind_size_; wind_capacity_lost = true;
      }
      wind_samples_[(wind_head_ + wind_size_) % wind_samples_.size()] = {s.time_ms, s.wind.value};
      ++wind_size_;
      if (!wind_seen_) { wind_seen_ = true; wind_start_ = s.time_ms; }
      if (last_wind_time_ && s.time_ms - last_wind_time_ > 60000) wind_start_ = s.time_ms;
      last_wind_time_ = s.time_ms;
    }
    uint16_t rain_delta = 0;
    bool rain_continuous = false;
    if (s.rain_tips.valid) {
      const uint32_t previous_total=session_rain_tips,previous_discontinuities=rain_discontinuities;
      const uint64_t interval=rain_baseline?s.time_ms-rain_time_:0;
      if (rain_baseline) {
        const uint64_t gap = s.time_ms - rain_time_;
        const int delta = int(s.rain_tips.value) - int(rain_counter_);
        if (gap > kStaleMs) rain_incomplete = true;
        if (delta >= 0) {
          session_rain_tips += delta;
          rain_continuous = gap <= kStaleMs;
          if (rain_continuous) rain_delta = delta;
        } else if (rain_counter_ >= 500 && s.rain_tips.value <= 11 && gap <= kStaleMs) {
          // Conservative near-boundary wrap candidate; a reset at that boundary
          // cannot be distinguished without a sensor reset indicator.
          rain_delta = uint16_t(512 + delta);
          session_rain_tips += rain_delta;
          rain_continuous = true;
          rain_incomplete = true;
          ++rain_discontinuities;
        } else {
          ++rain_discontinuities; rain_incomplete = true;
        }
      }
      rain_timeline.record(s.time_ms,interval,session_rain_tips-previous_total,rain_continuous,rain_discontinuities!=previous_discontinuities);
      rain_baseline = true;
      rain_counter_ = s.rain_tips.value; rain_time_ = s.time_ms;
    }
    if (history_) {
      const uint64_t minute = s.time_ms / 60000;
      Bucket& b = history_[minute % kHistoryBuckets];
      if (b.minute != minute) { b = Bucket{}; b.minute = minute; }
      if (s.temperature.valid) { b.temperature_sum += s.temperature.value; ++b.temperature_count; }
      if (s.humidity.valid) { b.humidity_sum += s.humidity.value; ++b.humidity_count; }
      if (s.lux.valid) { b.lux_sum += s.lux.value; ++b.lux_count; }
      if (s.wind.valid) { if (!b.wind_count || s.wind.value > b.wind_max) b.wind_max = s.wind.value; ++b.wind_count; }
      if (rain_continuous) {
        // Allocate a continuous interval to the minute in which its counter
        // arrived. This preserves totals with up to one minute timing uncertainty.
        b.rain_tips += rain_delta; b.rain_observed = true;
      }
      if (s.rain_tips.valid) previous_rain_time_ = s.time_ms;
    }
    return Result::Accepted;
  }

  Field<float> gust(uint64_t now) {
    prune(now);
    Field<float> peak;
    for (size_t i = 0; i < wind_size_; ++i) {
      const auto& p = wind_samples_[(wind_head_ + i) % wind_samples_.size()];
      if (p.time_ms <= now && (!peak.valid || p.value > peak.value)) peak.set(p.value, p.time_ms);
    }
    return peak;
  }
  bool gustComplete(uint64_t now) const {
    return wind_seen_ && now >= wind_start_ && now - wind_start_ >= kGustWindowMs &&
           wind.fresh(now) && !wind_capacity_lost;
  }
  struct RainState { uint64_t time=0; uint32_t total=0,discontinuities=0; uint16_t counter=0; bool baseline=false,incomplete=true; };
  RainState rainState() const { return {rain_time_,session_rain_tips,rain_discontinuities,rain_counter_,rain_baseline,rain_incomplete}; }
  void restoreRain(const RainState& s) { rain_time_=s.time;previous_rain_time_=s.time;session_rain_tips=s.total;rain_discontinuities=s.discontinuities;rain_counter_=s.counter;rain_baseline=s.baseline;rain_incomplete=true; }
  struct HistorySummary {
    size_t buckets = 0;
    uint64_t first_minute = 0, last_minute = 0;
  };
  HistorySummary historySummary(uint64_t now) const {
    HistorySummary result;
    const uint64_t end = now / 60000;
    const uint64_t begin = end >= kHistoryBuckets - 1 ? end - kHistoryBuckets + 1 : 0;
    for (uint64_t minute = begin; minute <= end; ++minute) {
      const auto* b = bucket(minute);
      if (!b || !(b->temperature_count || b->wind_count || b->lux_count || b->humidity_count || b->rain_observed)) continue;
      if (!result.buckets) result.first_minute = minute;
      result.last_minute = minute;
      ++result.buckets;
    }
    return result;
  }
  const Bucket* bucket(uint64_t minute) const {
    if (!history_) return nullptr;
    const auto& b = history_[minute % kHistoryBuckets];
    return b.minute == minute ? &b : nullptr;
  }

 private:
  struct WindSample { uint64_t time_ms = 0; float value = 0; };
  std::array<WindSample, 512> wind_samples_{};
  Bucket* history_;
  uint16_t station_;
  char channel_;
  size_t wind_head_ = 0, wind_size_ = 0;
  Sample last_{};
  uint64_t rain_time_ = 0, previous_rain_time_ = 0, wind_start_ = 0, last_wind_time_ = 0;
  uint16_t rain_counter_ = 0;
  bool wind_seen_ = false;
  void prune(uint64_t now) {
    while (wind_size_) {
      const auto t = wind_samples_[wind_head_].time_ms;
      if (now < t || now - t < kGustWindowMs) break;
      wind_head_ = (wind_head_ + 1) % wind_samples_.size(); --wind_size_;
    }
  }
  template <typename T> static bool equal(const Field<T>& a, const Field<T>& b) {
    return a.valid == b.valid && (!a.valid || a.value == b.value);
  }
  static bool samePayload(const Sample& a, const Sample& b) {
    return a.type == b.type && equal(a.temperature,b.temperature) && equal(a.humidity,b.humidity) &&
           equal(a.wind,b.wind) && equal(a.direction,b.direction) && equal(a.lux,b.lux) &&
           equal(a.uv,b.uv) && equal(a.rain_tips,b.rain_tips) && equal(a.strikes,b.strikes);
  }
  static bool range(const Field<float>& f, float low, float high) {
    return !f.valid || (std::isfinite(f.value) && f.value >= low && f.value <= high);
  }
  static bool valid(const Sample& s) {
    return range(s.temperature,-40,158) && range(s.wind,0,200) && range(s.direction,0,360) &&
           range(s.uv,0,15) && (!s.humidity.valid || s.humidity.value <= 100) &&
           (!s.lux.valid || s.lux.value <= 120000) && (!s.rain_tips.valid || s.rain_tips.value <= 511);
  }
};
}  // namespace weather
