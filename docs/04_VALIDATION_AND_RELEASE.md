# Validation and release gates

## Evidence to collect while implementing

Record commands and observed results in the public software status and the local
development log. Build success proves compilation, not RF sensitivity, station
support or privacy.
Run host tests for identity separation, repeated/unknown packets, missing versus
zero, per-field freshness, unit conversions, escaping, maximum input sizes,
queue loss reporting, clock wrap/time sync and sequence accounting. Test storage
at its limit, interrupted records, reboot recovery and download interruption.
The reader must reject malformed input without executing or rendering it as HTML.

A bundle validator must check every JSONL line, schema version, unique session,
sequence ordering/gaps, event counts, pulse-array lengths and hashes. A missing
end record denotes an interrupted session; recover complete preceding lines and
report the condition. Never silently "repair" original evidence. Synthetic fixtures
must stay labeled synthetic in files, preview and manifest.

## Hardware acceptance after isolation

Use a spare unit or explicitly owner-authorized test unit after milestone 1.
Before flashing, confirm independent capture identity/NVS and no enabled path to
APRS, CWOP, WU, Weathercloud, NWS, or a generic/automatic updater. The OTA
exceptions are the user-confirmed fixed-target Connect update and Connect-to-WX-Link transition. Connect
settings stay isolated and are not copied to the main `wxlink` namespace. Preserve
electrical wiring and baseline receiver settings.

Compare one known Atlas with a concurrent SDR reference, logging antenna,
placement and applied profile. Verify repeated packets refresh Home age, preview
selection does not filter saved neighbors, unknown JSON remains intact, Wi-Fi
outage does not stop local capture, and exports remain bounded during RF load.
Check Home text at 320 × 240, missing fields, wake/touch, and browser mobile layout.
Exercise full storage, reboot/power interruption and session resume/close policy.
Record durations/counts/losses; do not call an unperformed multiday soak passed.

## Submission and release

Generate a local bundle containing manifest.json, decoded.jsonl, optional
pulses.jsonl, diagnostics.json and README.txt. Include sizes and SHA-256 hashes
for payload files; do not recursively hash the manifest into itself. Identify
firmware and decoder versions, applied RF settings and loss statistics. Field
notes may state station brand/model, sample period and weather changes without
requiring an address or account. Include photos only when voluntarily chosen.

Neighbor sensor IDs and timing may be sensitive even without GPS. Provide a
review step, explicit device selection for a separately generated submission
bundle, and a list of excluded records/counts. Keep the original local capture
unchanged. Never include Wi-Fi passwords, API keys, admin credentials, NVS dumps,
APRS passcodes, owner MAC/IP identifiers or precise location by default. Manual
submission is sufficient; no hosting service or automatic upload is required.

Retain GPL and third-party notices from all copied source/libraries/assets.
Document this as separate experimental capture firmware, supported hardware,
actual enabled decoders and known losses. A decoder listing is not verified
physical compatibility. Publishing, creating a fork and flashing remain separate
owner-directed actions. A software prerelease may be published before hardware
acceptance only when its candidate status and untested RF gates are explicit.

## Local bundle validation and privacy review

`tools/connect_capture.py` writes a `.partial` directory, validates it, then renames it
to the completed bundle path. The validator checks manifest schema and identity,
allowed payload paths, every SHA-256 and byte count, UTF-8/JSONL completeness, event
and pulse bounds, decoded sequence order/gaps, source consistency, session/profile
metadata, record counts, interrupted state, and explicit decoder/queue/storage/USB/raw
truncation totals. Diagnostics cannot contain known credential, token, NVS, MAC or IP
configuration keys. Optional user-entered station make/model, broad region, antenna,
supply, console comparison and observation notes have length bounds; credential terms
and precise coordinates are rejected. Do not enter an address or account information.

The exported bundle retains all captured devices by default and says that neighbor IDs
and timing may be present. It is local only. To create a reviewed derivative without
opening USB, run `python tools/connect_capture.py --review-bundle PATH`. Choose one
listed identity or `a` for all devices, choose local-only or manual public-review
intent, and type the session confirmation. A selected-device derivative omits
uncorrelated pulse records and records the excluded packet count; it cannot guarantee
that unknown native fields do not contain other identifiers. The optional redaction
step creates another derivative, replacing common ID keys (`id`, `device_id`,
`station_id`, `sensor_id`, `serial`, `serial_number`, `mac`, `mac_address`) with
`[REDACTED]` recursively in packet `native_json`; it documents changed payloads and
recalculates all checksums. Unknown key names can remain. The source bundle is never
modified. The tool does not transmit or submit either bundle; a public-review choice
is a recorded manual intent, not publication authorization.

## M12 software prerelease and hardware gates

A software prerelease may be published for initial owner-led device testing while
hardware acceptance remains pending. The prerelease is explicitly not hardware-
accepted or a validated production release. Its firmware asset is application-image-
only and includes no bootloader, partition table, NVS image, credentials or
participant captures. Publishing version 0.1.0 does not mark M12 complete.
The main-project transition downloads a compatible stable main-project release from the fixed
`sideout909/WX-Link` repository. HTTPS plus the release manifest hash validate the
download; there is no independent publisher signature. Hardware/RF acceptance and
device flashing remain pending.

The authorized `0.1.0` software prerelease is available from [WX-Link Connect releases](https://github.com/sideout909/WX-Link-Connect/releases). It is for owner-led initial device testing only; M12 remains partial until the listed hardware gates pass. The one-way installer details are in [Connect → main WX-Link](CONNECT_TO_MAIN_TRANSITION.md).
