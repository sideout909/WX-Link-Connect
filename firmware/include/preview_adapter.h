#pragma once

#include <ArduinoJson.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wxconnect::preview {
constexpr size_t kTextCapacity = 40;
enum class RainUnit : uint8_t { None, Inch, Millimetre };

struct Identity {
  std::array<char, 128> protocol{};
  std::array<char, kTextCapacity> model{}, id{}, channel{};
  bool has_protocol = false, has_model = false, has_id = false, has_channel = false;
};

struct Value {
  double value = 0;
  uint64_t time_ms = 0;
  bool valid = false, conflict = false;
};

struct Model {
  Identity identity;
  Value temperature_c, wind_mph, wind_direction_deg, humidity_pct;
  Value rain_counter, light_lux, uv_index;
  RainUnit rain_unit = RainUnit::None;
  uint32_t packets = 0;
  uint64_t last_packet_ms = 0;
  bool selected = false, adapter_available = false;
};

class Adapter {
 public:
  static bool supportsModel(const char* model) {
    return model && std::strcmp(model, "Acurite-Atlas") == 0;
  }
  bool select(const char* protocol, const char* model, const char* id, const char* channel) {
    Identity requested;
    if (!copy(protocol, requested.protocol, requested.has_protocol) ||
        !copy(model, requested.model, requested.has_model) ||
        !copy(id, requested.id, requested.has_id) ||
        !copy(channel, requested.channel, requested.has_channel))
      return false;
    model_ = Model{};
    model_.identity = requested;
    model_.selected = true;
    model_.adapter_available = supportsModel(requested.model.data());
    return true;
  }

  void clearSelection() { model_ = Model{}; }

  bool ingest(const char* native_json, size_t size, uint64_t uptime_ms) {
    if (!model_.selected || !model_.adapter_available || !native_json || !size) return false;
    StaticJsonDocument<2048> document;
    if (deserializeJson(document, native_json, size) || !document.is<JsonObjectConst>()) return false;
    const Identity received = readIdentity(document.as<JsonVariantConst>());
    if (!same(model_.identity, received)) return false;

    model_.last_packet_ms = uptime_ms;
    if (model_.packets != UINT32_MAX) ++model_.packets;
    updateTemperature(document.as<JsonVariantConst>(), uptime_ms);
    updateWind(document.as<JsonVariantConst>(), uptime_ms);
    updateSingle(document["wind_dir_deg"], model_.wind_direction_deg, uptime_ms, 0, 360, true);
    updateSingle(document["humidity"], model_.humidity_pct, uptime_ms, 0, 100, true);
    updateRain(document.as<JsonVariantConst>(), uptime_ms);
    updateSingle(document["lux"], model_.light_lux, uptime_ms, 0, 1.0e9);
    updateSingle(document["uv"].isUnbound() ? document["uvi"] : document["uv"],
                 model_.uv_index, uptime_ms, 0, 100);
    return true;
  }

  const Model& model() const { return model_; }

 private:
  template <size_t N>
  static bool copy(const char* source, std::array<char, N>& destination, bool& present) {
    destination.fill('\0');
    present = source && source[0];
    if (!present) return true;
    const size_t length = std::strlen(source);
    if (length >= N) return false;
    std::memcpy(destination.data(), source, length);
    return true;
  }

  static bool readText(JsonVariantConst value, char* out, size_t capacity) {
    out[0] = '\0';
    if (value.is<const char*>()) {
      const char* text = value.as<const char*>();
      const size_t size = std::strlen(text);
      if (size >= capacity) return false;
      std::memcpy(out, text, size + 1);
      return true;
    }
    if (!value.is<int64_t>() && !value.is<uint64_t>()) return false;
    const size_t size = serializeJson(value, out, capacity);
    return size < capacity;
  }

  static Identity readIdentity(JsonVariantConst native) {
    Identity identity;
    char protocol[128];
    if (readText(native["protocol"], protocol, sizeof(protocol))) copy(protocol, identity.protocol, identity.has_protocol);
    char value[kTextCapacity];
    if (readText(native["model"], value, sizeof(value))) copy(value, identity.model, identity.has_model);
    if (readText(native["id"], value, sizeof(value))) copy(value, identity.id, identity.has_id);
    if (readText(native["channel"], value, sizeof(value))) copy(value, identity.channel, identity.has_channel);
    return identity;
  }

  template <size_t N>
  static bool equal(const std::array<char, N>& a, bool have_a,
                    const std::array<char, N>& b, bool have_b) {
    return have_a == have_b && (!have_a || std::strcmp(a.data(), b.data()) == 0);
  }

  static bool same(const Identity& a, const Identity& b) {
    return equal(a.protocol, a.has_protocol, b.protocol, b.has_protocol) &&
           equal(a.model, a.has_model, b.model, b.has_model) &&
           equal(a.id, a.has_id, b.id, b.has_id) &&
           equal(a.channel, a.has_channel, b.channel, b.has_channel);
  }

  static bool number(JsonVariantConst value, double& out) {
    if (!value.is<double>() && !value.is<int64_t>() && !value.is<uint64_t>()) return false;
    out = value.as<double>();
    return std::isfinite(out);
  }

  static void assign(Value& target, double value, uint64_t now) {
    target = Value{value, now, true, false};
  }

  static void invalidate(Value& target, uint64_t now, bool conflict) {
    target.valid = false;
    target.time_ms = now;
    target.conflict = conflict;
  }

  void updateTemperature(JsonVariantConst native, uint64_t now) {
    double c = 0, f = 0;
    const bool has_c = number(native["temperature_C"], c);
    const bool has_f = number(native["temperature_F"], f);
    if (has_c && has_f && std::fabs(c - (f - 32.0) * (5.0 / 9.0)) > 0.2) {
      invalidate(model_.temperature_c, now, true);
    } else if (has_f) {
      assign(model_.temperature_c, (f - 32.0) * (5.0 / 9.0), now);
    } else if (has_c) {
      assign(model_.temperature_c, c, now);
    } else if (!native["temperature_C"].isUnbound() || !native["temperature_F"].isUnbound()) {
      invalidate(model_.temperature_c, now, false);
    }
  }

  void updateWind(JsonVariantConst native, uint64_t now) {
    double mph = 0, ms = 0, kmh = 0;
    const bool has_mph = number(native["wind_avg_mi_h"], mph);
    const bool has_ms = number(native["wind_avg_m_s"], ms);
    const bool has_kmh = number(native["wind_avg_km_h"], kmh);
    const double from_ms = ms * 2.2369362921;
    const double from_kmh = kmh / 1.609344;
    const unsigned sources = unsigned(has_mph) + unsigned(has_ms) + unsigned(has_kmh);
    bool conflict = (has_mph && has_ms && std::fabs(mph - from_ms) > 0.05) ||
                    (has_mph && has_kmh && std::fabs(mph - from_kmh) > 0.05) ||
                    (has_ms && has_kmh && std::fabs(from_ms - from_kmh) > 0.05);
    if ((has_mph && mph < 0) || (has_ms && ms < 0) || (has_kmh && kmh < 0))
      invalidate(model_.wind_mph, now, false);
    else if (conflict) invalidate(model_.wind_mph, now, true);
    else if (sources) assign(model_.wind_mph, has_mph ? mph : has_ms ? from_ms : from_kmh, now);
    else if (!native["wind_avg_mi_h"].isUnbound() || !native["wind_avg_m_s"].isUnbound() ||
             !native["wind_avg_km_h"].isUnbound()) invalidate(model_.wind_mph, now, false);
  }

  static void updateSingle(JsonVariantConst source, Value& target, uint64_t now,
                           double minimum, double maximum, bool maximum_inclusive = false) {
    if (source.isUnbound()) return;
    if (source.isNull()) { invalidate(target, now, false); return; }
    double value = 0;
    if (!number(source, value) || value < minimum ||
        (maximum_inclusive ? value > maximum : value >= maximum)) {
      invalidate(target, now, false);
      return;
    }
    assign(target, value, now);
  }

  void updateRain(JsonVariantConst native, uint64_t now) {
    double inches = 0, millimetres = 0;
    const bool has_inches = number(native["rain_in"], inches);
    const bool has_mm = number(native["rain_mm"], millimetres);
    if ((has_inches && inches < 0) || (has_mm && millimetres < 0) ||
        (has_inches && has_mm && std::fabs(inches - millimetres / 25.4) > 0.01)) {
      invalidate(model_.rain_counter, now, has_inches && has_mm);
    } else if (has_inches) {
      assign(model_.rain_counter, inches, now);
      model_.rain_unit = RainUnit::Inch;
    } else if (has_mm) {
      assign(model_.rain_counter, millimetres, now);
      model_.rain_unit = RainUnit::Millimetre;
    } else if (!native["rain_in"].isUnbound() || !native["rain_mm"].isUnbound()) {
      invalidate(model_.rain_counter, now, false);
    }
  }

  Model model_;
};
}  // namespace wxconnect::preview
