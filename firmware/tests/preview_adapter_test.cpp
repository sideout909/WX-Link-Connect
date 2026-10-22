#include "preview_adapter.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using wxconnect::preview::Adapter;
using wxconnect::preview::RainUnit;

int main() {
  Adapter adapter;
  assert(adapter.select("acurite", "Acurite-Atlas", "101", "A"));
  constexpr char first[] = R"({"protocol":"acurite","model":"Acurite-Atlas","id":101,"channel":"A","temperature_F":32,"humidity":0,"wind_avg_mi_h":0,"wind_dir_deg":0,"rain_in":0,"lux":0,"uv":0})";
  assert(adapter.ingest(first, sizeof(first) - 1, 1000));
  const auto& a = adapter.model();
  assert(a.selected && a.adapter_available && a.packets == 1);
  assert(a.temperature_c.valid && std::fabs(a.temperature_c.value) < 1e-9);
  assert(a.humidity_pct.valid && a.humidity_pct.value == 0);
  assert(a.wind_mph.valid && a.wind_mph.value == 0);
  assert(a.wind_direction_deg.valid && a.wind_direction_deg.value == 0);
  assert(a.rain_counter.valid && a.rain_counter.value == 0 && a.rain_unit == RainUnit::Inch);
  assert(a.light_lux.valid && a.light_lux.value == 0);
  assert(a.uv_index.valid && a.uv_index.value == 0);

  constexpr char second[] = R"({"protocol":"acurite","model":"Acurite-Atlas","id":101,"channel":"A","temperature_C":0,"humidity":100})";
  assert(adapter.ingest(second, sizeof(second) - 1, 2000));
  assert(adapter.model().temperature_c.time_ms == 2000);  // Stable value refreshes age.
  assert(adapter.model().humidity_pct.valid && adapter.model().humidity_pct.value == 100);
  assert(adapter.model().wind_mph.time_ms == 1000);  // Omitted field keeps its own age.

  Adapter cotech;assert(cotech.select(nullptr,"Cotech-367959","1",nullptr));
  constexpr char light[]=R"({"model":"Cotech-367959","id":1,"light_lux":12345,"uvi":3.2,"wind_avg_m_s":1})";
  assert(cotech.ingest(light,sizeof(light)-1,1000));
  assert(cotech.model().light_lux.valid&&cotech.model().light_lux.value==12345);
  constexpr char light_conflict[]=R"({"model":"Cotech-367959","id":1,"lux":10,"light_lux":11})";
  assert(cotech.ingest(light_conflict,sizeof(light_conflict)-1,2000));
  assert(!cotech.model().light_lux.valid&&cotech.model().light_lux.conflict);

  constexpr char uv_conflict[]=R"({"model":"Cotech-367959","id":1,"uv":1,"uvi":2})";
  assert(cotech.ingest(uv_conflict,sizeof(uv_conflict)-1,3000));
  assert(!cotech.model().uv_index.valid&&cotech.model().uv_index.conflict);
  assert(!Adapter::supportsModel("RainPoint-Soil"));
  assert(!Adapter::supportsModel("Homelead-HG9901")); // Soil moisture is not air humidity.
  assert(!Adapter::supportsModel("RainPoint-HCS012ARF")); // Native capture only: unresolved rain scaling.

  Adapter nullable;nullable.select("acurite","Acurite-Atlas","101","A");
  assert(nullable.ingest(first,sizeof(first)-1,1000));
  constexpr char nulls[]=R"({"protocol":"acurite","model":"Acurite-Atlas","id":101,"channel":"A","temperature_F":null,"humidity":null,"wind_avg_mi_h":null,"wind_dir_deg":null,"rain_in":null,"lux":null,"uv":null})";
  assert(nullable.ingest(nulls,sizeof(nulls)-1,2000));
  const auto& n=nullable.model();
  assert(!n.temperature_c.valid && !n.humidity_pct.valid && !n.wind_mph.valid);
  assert(!n.wind_direction_deg.valid && !n.rain_counter.valid && !n.light_lux.valid && !n.uv_index.valid);
  constexpr char north[]=R"({"protocol":"acurite","model":"Acurite-Atlas","id":101,"channel":"A","wind_dir_deg":360,"wind_avg_mi_h":-1})";
  assert(nullable.ingest(north,sizeof(north)-1,3000));
  assert(nullable.model().wind_direction_deg.valid && nullable.model().wind_direction_deg.value==360);
  assert(!nullable.model().wind_mph.valid);

  constexpr char other[] = R"({"protocol":"acurite","model":"Acurite-Atlas","id":202,"channel":"A","temperature_F":90})";
  assert(!adapter.ingest(other, sizeof(other) - 1, 3000));
  assert(adapter.model().temperature_c.value == 0 && adapter.model().packets == 2);

  assert(adapter.select("acurite", "Acurite-Atlas", "202", "A"));
  assert(!adapter.model().temperature_c.valid && !adapter.model().humidity_pct.valid);
  assert(adapter.ingest(other, sizeof(other) - 1, 4000));
  assert(adapter.model().temperature_c.valid && std::fabs(adapter.model().temperature_c.value - 32.2222222) < 0.01);
  assert(!adapter.model().humidity_pct.valid);  // Selection never inherits A's fields.

  constexpr char conflict[] = R"({"protocol":"acurite","model":"Acurite-Atlas","id":202,"channel":"A","rain_in":1,"rain_mm":30})";
  assert(adapter.ingest(conflict, sizeof(conflict) - 1, 5000));
  assert(!adapter.model().rain_counter.valid && adapter.model().rain_counter.conflict);

  assert(adapter.select("unknown", "Unmapped-Weather", "x", ""));
  assert(!adapter.model().adapter_available);
  assert(!adapter.ingest(first, sizeof(first) - 1, 6000));
  assert(!adapter.model().temperature_c.valid);
  assert(adapter.select(nullptr, nullptr, "unbound", nullptr));
  assert(adapter.model().selected && !adapter.model().adapter_available);
  std::puts("Preview adapter checks passed: identity isolation, missing fields, zero/100, independent ages, unit mapping and unknown models.");
}
