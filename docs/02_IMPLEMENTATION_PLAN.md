# Ordered implementation work

This is the ordered implementation plan. The current public status and validation
evidence are in [RELEASE_STATUS.md](RELEASE_STATUS.md); the local development log
is maintained separately. Files named below are existing sources unless marked
NEW. The MVP is milestones 1–8 with truthful OOK scope; raw-pulse and profile work
is required before advertising unknown-device research support. No hardware upload
is implied by a passing build. Do not implement internet submission in any milestone.

## 1. Isolate the copied application

Inspect the build target, application identity, settings allowlist and source
filters. Change app identity/version to WXLink Connect 0.1.0 and a separate
build/release identity. Use NVS namespace wxconnect; preserve
old wxlink settings/files and never read their uploader credentials. Remove/exclude
weather exporters and production OTA from the Connect build. Also remove calls that
can invoke them; hiding controls or leaving default enable flags false is insufficient.
Use /connect/ data paths, not existing archive/checkpoint filenames. Start recording
only after explicit user action. Keep RF/display/USB configuration unchanged.

Checks: build succeeds; source/build call graph contains no reachable observation
uploader or generic/automatic production update client; old uploader settings cannot
activate anything. The later owner-authorized one-way transition is a separate fixed-
target action and does not restore the generic updater.
Test a synthetic old config with every exporter enabled. Do not flash it. Record the
exact exclusion method and remaining baseline modules. Gate: app identity and isolation
proven before adding new capture functionality.

## 2. Define capture models and queues

Create include/capture_types.h, capture_session.h and tests/capture_session_test.cpp.
Follow docs/03 and schemas/capture-v1.schema.json. Use monotonic 64-bit time; UTC is
nullable. Session IDs are random, record sequence starts at zero, and every record
has a source label. Native JSON is copied exactly, not regenerated from weather fields.
Keep repetitions. Add fixed capacities and counters for oversize/queue-full/invalid
JSON/storage drops. Initially allow 2,048 native bytes including NUL, queue depth four;
compute full envelope expansion separately (escaping can double the string).

Checks: session start/stop, sequence, wrap-safe time, UTC invalid->valid, repeats,
unknown fields, exactly-at-limit/oversize and queue-full behavior. No Arduino dependency
in the model. Gate: contract tests pass with synthetic records and explicit size budgets.

## 3. Record all decoded callbacks before station filtering

Read radio_receiver.cpp and main.cpp::liveReceive. Extend the existing bounded receiver
copy path if needed. Deliver callback bytes to the capture broker before station_confirmed,
atlas::decode, discovery eligibility, identity filtering or weather deduplication. One bad
JSON callback may count as rejected, but cannot stop later records. Build a fixed candidate
table of at most 32 identities; preserve model/ID/channel/protocol fields where available.
Never merge missing identities into one confidently identified station.

Checks: inject Atlas plus foreign/unknown callbacks; all eligible native records reach
capture regardless of preview selection. Missing model/ID remains representable. Repeated
copies appear separately. Gate: Atlas-specific code is not a capture prerequisite.

## 4. Implement the familiar live Home preview

Read renderHome and header helpers, docs/03, and concepts/connect-home.svg. Create a small
preview adapter/model separate from recording. Start with reviewed AcuRite native mappings;
unknown decoders still show identity/counts/raw keys. Show real temperature, wind, humidity,
raw rain counter, light/UV where present. Each field keeps its own timestamp and unit.
Select a preview sensor on device/browser, never by strongest RSSI. Auto-preview a sole
candidate is allowed only with its identity shown; multiple candidates require selection.

Checks: select A then B, verify fields/ages never mix; missing remains --; real zero survives;
same-value packets update reception age. Rain is labeled counter, not today/session rain.
Capture continues for every device while preview changes. Gate: the Home looks familiar
and visibly demonstrates reception without false station-support or weather claims.

## 5. Add bounded raw-pulse capture

Read setRawPulsesCallback declaration and its call site in vendor/rtl_433_ESP/src/signalDecoder.cpp.
Register the existing hook; do not first rewrite the receiver ISR. Copy pulse/gap arrays
inside the callback into a separate bounded queue. Proposed initial cap: 512 pairs/record,
two queued frames; record original count, captured count and truncated flag. Validate int
ranges before conversion. Document memory and sampling limits. Rate-limit raw capture
separately (default off, explicit short sample window), count skipped/dropped frames, and
never claim one pulse record equals one decoded packet. Correlation stays null unless an
upstream frame ID is genuinely carried through both callbacks.

Checks: unknown/zero-decode signals, cap/truncation, callback lifetime, queue pressure and
stop during activity. Raw pointers never escape the callback. Gate: undecoded evidence can
be exported without blocking reception or falsely claiming raw-IQ coverage.

## 6. Add bounded session storage

Create capture_storage.cpp with a fixed budget, CRC/recovery strategy and own files.
Proposed decoded pool 256 KiB; optional raw pool 256 KiB; keep at least 512 KiB filesystem
free. Verify actual partition/free space first. If old production data occupies the space,
refuse capture with an actionable message; never erase it automatically. Rotate bounded
segments, preserve explicit retention/drop counts, and never overwrite another session
silently. Avoid a flash write for every RF repetition: bounded batches with a documented
flush/loss window. Handle reset and interrupted final JSONL lines.

Checks: full storage, reserve threshold, partial final write, CRC failure, restart,
sequence gaps and download concurrent with recording. Gate: storage cannot grow without
bound or make packet loss invisible. Update precise capacity estimates using measured sizes.

## 7. Implement the USB collector and portable bundle

Build NEW tools/connect_capture.py and tests around the existing USB console
protocol; keep radio diagnostics separate from packet collection.
Require explicit --port or enumerate ports for user selection; remove the owner's serial
as a distribution default. One exclusive port, acknowledged commands with request IDs,
stream heartbeat/lease and guaranteed cleanup on Ctrl-C/unplug. Commands/status use a
stable envelope; do not reuse Atlas-only RF export as the new general transport.

Bundle layout: manifest.json, decoded.jsonl, optional pulses.jsonl, diagnostics.json and
README.txt. Host compression/checksums are fine; do not allocate a ZIP of all data in ESP RAM.
Preserve native_json exactly and nullable UTC. Write to .partial, validate, then atomically
publish the complete bundle. Never overwrite an existing capture directory.

Checks: wrong reply ID, periodic status noise, malformed/truncated lines, reconnect,
read-only port errors, sequence loss and interrupted export. Gate: a user can collect a
reviewable bundle without knowing a station ID or running cloud software.

## 8. Implement protected browser capture controls

Reuse protected setup/auth/CSRF and local assets. Replace weather sharing/NWS/general-updater tabs
with Overview, Capture, Devices and Setup. Overview resembles the Home preview. Capture
shows Ready/Recording/Stopped/Error, session age, profile, packet/pulse/drop counts and
storage remaining. Provide Start, Stop and paged Download; raw capture needs an explicit
checkbox and short-duration limit. Devices selects preview target and shows discovered
identities. Setup retains Wi-Fi/time/display/security; location/callsign are not required.

Suggested APIs: GET /api/connect/status, GET /api/connect/devices, POST /api/connect/start,
POST /api/connect/stop, GET /api/connect/export?session=...&offset=.... Define bounded
pagination/revision semantics before implementation. Validate identifiers, auth, CSRF and
request sizes. Use textContent for RF strings. No upload endpoint to an external service.

Checks: new-user setup-first, mobile layout, malicious RF text, repeated saves, start/stop
races, download while receiving, missing UTC, and interrupted request recovery. Gate:
volunteers can operate the collector from a phone with useful feedback, not silent buttons.

## 9. Enable a reviewed OOK decoder subset

Read [DECODER_PROFILE.md](DECODER_PROFILE.md), `reference/decoder-inventory/433-weather.csv`,
`atlas_devices.h` and the actual `r_device` sources.
Build NEW connect_devices.h instead of blindly removing MY_DEVICES. Register one reviewed
subset and record its exact symbols/revision in session metadata. Start acurite_txr;
expand incrementally to documented 433 MHz OOK candidates with build/memory checks.
Do not enable FSK decoders in an OOK-only configuration. Disabled upstream decoders and
known CRC/rain-scaling defects require explicit experimental labels, not support claims.

Checks: build time/size/heap, known Atlas still decodes, registry count agrees with symbols,
unknown samples remain capturable. Gate: declared scope exactly matches enabled code;
a list of upstream decoders is never presented as verified retail compatibility.

## 10. Add deliberate receiver profiles, then FSK

Define profile ID, center frequency, modulation, bandwidth, bitrate/deviation where needed,
AGC and margin. Separate requested settings from verified/applied values. First preserve
existing OOK settings; a profile change stops/drains RF safely, increments profile revision,
records the transition and restarts the receiver. Never label queued old-profile packets
with new settings. Keep tuning unavailable until return codes/readback are checked.

FSK needs a known supported sensor/capture and actual decoder/radio review. Do not assume
calling setFrequency changes modulation. If the API cannot safely change mode at runtime,
ship separate builds/profiles and explain that boundary. Automatic sweeps are deferred.
Checks: transition boundary, failed config/recovery, no mixed profile records, wrong region,
concurrent stop/download. Gate: no false OOK+FSK simultaneous-reception claim.

## 11. Validate evidence quality and privacy

Implement bundle validator and tests in docs/04. Validate schema/bounds/checksums/sequence,
explicit truncation/drop totals and usable metadata. Add make/model/region/antenna/supply,
console comparison and observation notes to manifest; those are user-entered, not inferred.
Explain that neighboring sensor IDs/raw payloads may be present. Offer selected-device
exports and explicit all-device review; keep local originals. Redaction creates a derivative
with a documented transformation and new checksums, never rewrites native originals silently.

Gate: no Wi-Fi/admin/upload keys, NVS, full flash, private logs or exact location are in a
normal bundle. No automatic submission. Unknown native payloads cannot be guaranteed free
of identifying data; require user review and an explicit public-sharing choice.

## 12. Hardware acceptance and release

Use a spare device or owner-authorized hardware window, not the working weather appliance
without approval. Test known Atlas and at least one foreign/unknown-source example; compare
with SDR/reference console, record antenna/profile/time, run a controlled hour capture,
interrupt power/USB/network and verify recoverable evidence. Test UI during RF and downloads.
Measure flash/heap/PSRAM and queue drops. Verify radio is receive-only, no observation
uploader or generic updater is reachable, and only the fixed, explicitly confirmed
Connect-to-WX-Link migration route is available. Do not claim unsupported bands or
untested stations.

Then write a short volunteer guide, wiring/build instructions, known limitations, issue
submission template and manual review procedure. Package app-only binaries and notices
with distinct Connect manifest/repository identity. No credentials or full-flash images.
A GitHub repository/fork/release and contact with volunteers need owner direction. The
owner may authorize a clearly marked software prerelease before device acceptance; that
does not close M12 or establish RF/hardware compatibility. Gate: all required acceptance
results are recorded honestly, unfinished FSK/raw limitations are explicit, and Connect
artifacts cannot be installed through the main weather firmware's ordinary updater.
