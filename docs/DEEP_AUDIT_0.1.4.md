# Local 0.1.4 software audit

## Status and scope

`0.1.4-dev` is a local software candidate. No device, USB port, private capture,
flash backup, original-project source or settings was read during this audit.
No flashing, GitHub push, release publication or participant submission occurred.
Read-only Internet requests fetched public upstream samples and the public main
WX-Link release for comparison. The device restored in the preceding task was
not accessed or changed here. M12 remains partially complete.

## What accepts stations other than Atlas

`firmware/include/connect_devices.h` and `connect_decoder_profile.h` enable
exactly 20 decoder symbols, all for the fixed 433.92 MHz OOK profile. Every
accepted native callback is copied into a bounded pool. In
`firmware/src/main.cpp:liveReceive`, capture and the live packet view run before
preview parsing, selected identity matching, station confirmation, the legacy
Atlas adapter or weather-store deduplication. Preview selection never filters
recording. Unknown valid native JSON remains recordable without a preview.

This audit compiles the actual 20 vendored C decoder copies and their pulse
slicers on the host. It does not substitute the installed desktop rtl_433 decoder
for Connect's decoders. Public IQ was converted once to pulse/gap measurements
using desktop rtl_433 file input; the checked-in `.ook` fixtures permit later
runs without network access, SDR hardware or desktop rtl_433. Decoder callbacks
then use the actual checked formatter, followed by Connect capture/preview tests.
This checks post-demodulator software, not the CC1101 RF frontend, RSSI gate,
interrupt timing, antenna, transmitter variants or real reception rate.

The public corpus is [merbanan/rtl_433_tests](https://github.com/merbanan/rtl_433_tests),
pinned by revision/tree object `1aa8c29d6b9383739d1aeed495dbc5069b704392`.
Source paths, hashes, extracted-pulse hashes and extraction commands are in
`firmware/tests/fixtures/public-rtl433/provenance.json`. Expected outputs come
from independent upstream reference JSON; tests compare native output counts,
models, IDs and reference fields. These are publicly shared historical recordings,
not new Connect live evidence. Test recording envelopes are marked `replay`.

| Enabled decoder exercised | Public samples | Native callbacks verified |
| --- | --- | ---: |
| `acurite_txr` | [Acurite-Tower](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/acurite/Acurite_592TXR/acurite-592txr-003.cu8), [Acurite-5n1](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/acurite/Acurite_5-n-1/VN1TXC_02.cu8), [Acurite-3n1](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/acurite/Acurite_3-in-1/02/g001_433.92M_250k.cu8) | 8 |
| `ambient_weather` | [Ambientweather-F007TH](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/ambient_weather/01/f007th.cu8) | 5 |
| `auriol_afw2a1` | [Auriol-AFW2A1](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/auriol/Auriol_AFW2A1/g017_433.91M_250k.cu8) | 1 |
| `cotech_36_7959` | [Cotech-367959](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/cotech/cotech_36-7959/01/g001_433.92M_1000k.cu8) | 1 |
| `esperanza_ews` | [Esperanza-EWS](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/esperanza_ews/gfile004.cu8) | 1 |
| `fineoffset_wh1080` | [Fineoffset-WHx080](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/fineoffset/fineoffset_wh1080/01/gfile001.cu8) | 1 |
| `gt_wt_02` | [GT-WT02](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/Globaltronics/GT-WT-02/01/gfile003.cu8) | 6 |
| `gt_wt_03` | [GT-WT03](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/Globaltronics/GT-WT-03/01/g001_433.92M_250k.cu8) | 10 |
| `lacrossetx` | [LaCrosse-TX](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/lacrosse/tx6u_tx7u/gfile001.cu8) | 2 |
| `lacrossews` | [LaCrosse-WS2310](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/lacrosse/ws2310/gfile001.cu8) | 1 |
| `schou_72543_rain` | [Schou-72543](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/Schou/g1083_433.89M_250k.cu8) | 1 |
| `ttx201` | [Emos-TTX201](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/ttx201/01/g001_433.92M_250k.cu8) | 1 |
| `vauno_en8822c` | [Vauno-EN8822C](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/vauno-en8822c-1/01/g1148_433.92M_250k.cu8) | 1 |
| `wssensor` | [Hyundai-WS](https://github.com/merbanan/rtl_433_tests/blob/1aa8c29d6b9383739d1aeed495dbc5069b704392/tests/wssensor/01/g001_433.92M_250k.cu8) | 1 |

There are **16 waveform fixtures, 14 enabled decoder symbols, 16 model names and
40 native callbacks**. Each callback is also recorded twice in the capture test,
with byte-identical JSON, stable-value age refresh and selected-identity checks.
Each fixture additionally tests that an unknown model still records while it
cannot alter the selected preview. The suite verifies that all 20 compiled
protocol names fit the bounded identity field, including symbols lacking waveforms.
It cannot recover RF repeats already combined inside a native decoder; it proves
that Connect does not discard repeated native callbacks.

Six enabled symbols remain source-reviewed rather than waveform-tested here:
`emos_e6016`, `emos_e6016_rain`, `homelead_hg9901`, `rainpoint`,
`rainpoint_hcs012arf`, and `telldus_ft0385r`. Public EMOS bit codes and vendored
examples were found, but they are not counted as completed waveform tests.
Homelead and RainPoint soil data are captured without mapping soil moisture to
air humidity. RainPoint HCS012ARF remains capture-only for weather preview because
of the [reported rain scaling defect](https://github.com/merbanan/rtl_433/issues/3698).
Its native values are retained unchanged rather than silently corrected.
A symbol's inclusion is not hardware verification of every model/rebrand it names.

## Fixes made

### Non-Atlas Home and station identity

`preview_adapter.h` previously supported only Acurite-Atlas. It now recognizes
24 exact, source-reviewed weather schemas from the enabled set. The tests exercise
Iris/5n1, Notos/3n1, Tower, Ambient, Auriol, Cotech, Fine Offset, La Crosse,
Esperanza, GT-WT02/03, Schou, TTX201, Vauno and Hyundai native outputs. Absent
measurements stay unavailable. Temperature/wind conversions affect preview only;
rain remains the decoder's raw reported counter, never inferred daily rainfall.
Sources with known unresolved rainfall interpretation are not normalized in capture.

The 127-byte protocol limit still rejected Schou's 140-byte decoder description.
The shared protocol bound is now 191 bytes, with an automated fit check covering
all 20 compiled names. Model/ID/channel bounds remain separate; recording is
independent of candidate-table bounds. Cotech `light_lux` is mapped alongside
Atlas `lux`; contradictory light/UV aliases invalidate the preview value rather
than selecting one silently. Before initial automatic selection, two recognized
identities require a user choice; an existing selection remains sticky and an
unknown neighbor cannot hide a sole supported device.

### Oversized RF pulse trains and loss evidence

The primary ISR's pulse index could wrap at 1,200, overwriting earlier timings
and later offering a clipped train to a decoder as complete. It now reserves the
trailing slot, marks overflow once, increments the loss counter immediately and
discards that train at completion. No flash, network, UI work or callback allocation
was added. The existing native JSON formatter truncation checks remain in force.

`radio_receiver::dropped()` now includes library capture/decoder-queue losses as
well as application queue losses, so session receiver-loss totals include them.
`appDropped()` preserves the specific USB application-queue diagnostic. Upstream
revision, enabled symbols and an explicit local receiver patch revision are
recorded in session metadata. The bundle validator accepts older captures without
the new optional revision and rejects invalid new revision values. Public pulse
replays do not exercise the physical ISR: overflow policy tests and compilation
are the available software evidence; a hardware stress check remains required.

### Connect-to-main release lookup and installation

The old manifest rule compared the main project's release number to Connect's
release number and could reject a `-dev` Connect build outright. These are
independently versioned products. The target must now be a strictly numeric stable
main release at the fixed main-repository URL; Connect's version is not a target
upgrade/downgrade comparator. Board, partition, slot size, flash size, HTTPS,
allowed redirect hosts, typed confirmation, image header and SHA-256 checks remain.

The pinned HTTPClient destructor can call a borrowed TLS pointer after some
failed/disconnected `end()` paths. TLS is now declared first and destroyed last,
so the HTTP client cannot outlive it. Lookup failures previously had only a generic
message. Network status now includes request stage, DNS timeout or TLS/HTTP error;
failed checks release the busy state and permit retry. The exact cause of the
owner's earlier stall has not been reproduced on hardware.

DNS uses a five-second bounded request on the TCP/IP thread, with permanent,
ticketed callback state; late callbacks cannot reference an expired task-stack
IPAddress or complete a newer lookup. Header requests have a 45-second transport
deadline checked on I/O, plus the existing bounded TCP/TLS operations. Manifest
bodies have a 15-second total/three-second idle limit, and are parsed only after
bounded receipt. Fragmented image headers are accumulated with explicit deadlines.
Certificates still use the bundled CA roots and the original hostname/SNI,
not an IP-based certificate check or insecure TLS.

A read-only host check fetched the main `v0.8.0` manifest and 1,736,176-byte image,
verified the manifest SHA-256, ESP32-S3 header and application descriptor magic.
A frozen public manifest is accepted by the real firmware policy in host tests;
wrong repositories and inadequate slots are rejected. This verifies server format
and host transport only; it does not prove ESP32 TLS/OTA writing or main boot.

### SDK Wi-Fi credential storage

Connect Preferences were isolated, but the SDK still defaulted to persistent
Wi-Fi configuration. Initialization now disables SDK persistence/reconnect before
mode initialization, requires RAM storage, and clears inherited STA configuration
in RAM before using Connect's explicitly loaded credentials. Failures stop setup
before AP/STA credentials could be written to shared SDK flash storage. There is
no zero-argument `WiFi.begin()` or legacy-credential migration. Only Connect's
namespace persists its credentials; original NVS is not erased or read by app
settings code. SDK radio calibration handling is not a user-settings namespace.
The new initialization still needs device setup/reconnect acceptance testing.

## Verification and limits

Commands run successfully (logs in ignored `artifacts/deep-audit/`):

```bash
bash tools/pio.sh run -d firmware                 # baseline, before dependency changes; no upgrades made
bash tools/test-firmware.sh                      # full offline host suite, including public replay
python3 tools/test-public-decoders.py --sanitize  # ASan + UBSan, pinned decoders and capture/preview
python3 tools/check-connect-isolation.py         # source/storage and linked exporter/OTA exclusions
# Existing read-only local Playwright environment:
/home/lschaar/.local/share/cyd-weather-link/venv/bin/python tools/verify_connect_page.py
git diff --check
```

Build: RAM **173,112 / 327,680**, flash **1,381,637 / 3,342,336** bytes.
Largest application compiler-reported stack frame: `liveReceive`, 6,624 bytes;
loop allocation remains 24 KiB. Migration `runMigration` frame is 3,712 bytes;
worker allocation remains 16 KiB. Individual frame sizes are not a proof of total
runtime call-chain stack use. Binary is 1,382,000 bytes; SHA-256:
`a739511b9f8174b1c1513c96b053e535e6e70ffd440e0846851ee3036cfaddb2`.

Remaining hardware acceptance: cold setup/reconnect with RAM-only SDK settings,
scan/retry under load, sustained multi-station RF capture and real Home readouts,
oversized-pulse loss behavior, session recovery/download under RF load, and the
complete protected Connect-to-main TLS/download/install/boot flow. Six symbols
still need waveform fixtures and every non-Atlas station needs CC1101 validation.
One radio still receives one OOK profile; FSK and profile switching remain deferred.
The two cosmetic requests remain unchanged in `SMALL_UI_TODO.md` as instructed.
