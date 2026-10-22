# Capture and Home-screen contracts

## Familiar Home screen is required

Use the current weather display as the visual starting point: same 320 × 240
landscape layout, typography, spacing and palette system. See
`concepts/connect-home.svg` for fictional example values, not captured evidence.
This is a capture utility with a useful weather preview, not a blank packet counter.

- Header, y=0–43: WX CONNECT, selected model/ID, last received packet age, RF,
  Wi-Fi and recording indicators. Do not show APRS or sound. If space is tight,
  shorten the model label; never overlap status indicators.
- Main area, y=44–135: large outdoor temperature with units; wind speed and
  direction beside it; optional light/UV when that device actually reports them.
  Keep the familiar sparkline only when real, timestamped preview history exists.
- Tiles, y=140–183: humidity, explicitly labeled RAIN COUNTER, packet count.
  Rain counter is the station's cumulative reported counter with its source unit.
  Do not label it rain today or calculate rainfall from one observation.
- Bottom, y=190–234: capture state, saved/dropped counts, selected preview identity
  and an affordance to Capture/Devices. Replace the NWS alert box with this panel.
- No NWS requests, moon, forecast icon, fake alerts or invented barometer values.
  Reuse fonts, colors, dividers, cards and weather measurement rendering.

Show `--` for unavailable fields. No packets means "Waiting for packets", not
0 degrees. Show "Receiving · Ns" after a valid selected-device observation.
No selected-device packets for five minutes means "No recent packets"; color
retained values amber and show their age. Repeated identical packets refresh
reception age and are recorded. Unchanging weather alone never means frozen.
Each displayed measurement has its own validity/time; receiving only wind must
not refresh temperature age. Long IDs may be shortened visually but must remain
complete in the protected Devices page and capture records.

Initially auto-preview the first repeatedly seen weather-capable identity
(three packets is the suggested threshold). Let the user explicitly select another.
Identity includes model, ID and channel when present; absent ID is explicitly
unknown, not a fabricated ID. Selection never filters the capture stream.
Unknown models remain visible in Devices with counts and native fields even
when no preview adapter exists. Never merge two stations merely by model name.

## Preview adapter

Implement a host-testable adapter separate from the capture writer. Read actual
native keys. For example temperature_C may convert to F using C*9/5+32;
temperature_F is already F. wind_avg_mi_h is mph; wind_avg_m_s converts using
2.2369362921. Do not assume an unlabeled wind field has a unit. Missing, null,
nonfinite and invalid-type values are unavailable. Preserve legitimate zero.
Prefer canonical C and m/s inside the preview model with explicit validity,
source identity and per-field monotonic timestamps. Resolve conflicting source
units explicitly in the adapter tests; never silently overwrite contradictory data.
Temperature and wind/gust show one decimal, rain counter two decimals, humidity
an integer. Display rounding must not alter saved native precision. A derived
five-minute gust is a maximum of received wind observations for that device;
label it separately from a station-reported gust and do not claim full coverage.

## Capture envelope version 1

Each JSONL line is one complete UTF-8 JSON object. Schema lives in
`schemas/capture-v1.schema.json`; synthetic examples live in `fixtures/synthetic/`.
Required common fields:

| Field | Contract |
| --- | --- |
| schema | Integer 1 |
| event | session, packet, pulse, status, gap, or end |
| session_id | Random 32 lowercase hexadecimal characters; no owner identity |
| seq | Monotonic event sequence within a session |
| uptime_ms | Nonnegative 64-bit monotonic milliseconds, extended across millis wrap |
| utc_ms | UTC epoch milliseconds when valid, otherwise null |
| time_valid | Boolean; exactly agrees with utc_ms availability |
| source | live, synthetic, or replay |
| profile_id | Stable configured radio profile identity; initialization acceptance and register readback are separate fields |

For packet events, `native_json` is the exact decoder JSON string (excluding
its transport NUL), escaped into the outer JSON. Preserve unknown keys, numeric
precision and every repeat. Parsing it for preview never changes this string.
Decoder truncation must be counted and clearly marked; never write a silently
truncated record as if complete. Queue length and encoded-envelope length differ:
a 2047-byte native string can require much more space after JSON escaping.

Session metadata records firmware/build identity, decoder list, hardware profile,
frequency, modulation, bandwidth, RSSI policy and explicit capture limits. Record
requested and actual radio values separately. Do not dump NVS/config wholesale.
Status/gap records report queue overflow, truncation, storage limits, pulse window
and radio transitions. Reserve reporting capacity so a full queue cannot conceal
its own losses. No capture collection begins until the user explicitly starts it.

Pulse records have `pulse_us` and `gap_us` integer arrays of equal captured length,
`original_count`, `captured_count`, `truncated`, `duration_us`, optional native
`rssi_dbm`, and `packet_seq: null` unless a real correlation mechanism exists.
They are envelope timings, not IQ samples. Raw callbacks can follow JSON callbacks;
callback order alone cannot associate a pulse with a packet. Capture pulse data
only in a short, explicit, rate-limited window. Report pulses omitted by limits.

The defaults proposed in the implementation plan are starting bounds to measure,
not verified RAM/storage capacity. Stop cleanly at capacity; do not overwrite an
older participant session. Replay/synthetic input must be visibly labeled and
must never reach external weather services or masquerade as live evidence.
