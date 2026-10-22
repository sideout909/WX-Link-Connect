# WXLink Connect software status

## 0.1.5 release — owner-confirmed OTA paths

The owner reported successful Connect 0.1.5-dev → 0.1.5 OTA installation/reboot,
then a successful one-way upgrade to main WX-Link. Main required its own
credentials as designed; Connect credentials were not imported. The owner then
explicitly authorized promoting the same tested assets to a regular
[0.1.5 release](https://github.com/sideout909/WX-Link-Connect/releases/tag/v0.1.5).
The GitHub prerelease flag is now false. Application and manifest assets were
not rebuilt or replaced during promotion; their verified SHA256 remains unchanged.

These are owner-confirmed functional update tests, not a recorded power-loss,
rollback, storage-retention or multiday stress campaign. Wider RF/station checks
remain pending and M12 is still partially complete. Source notes below retain
the earlier candidate evidence and acceptance limits in chronological context.
No device was flashed during release preparation/publication; the owner performed
the OTA tests. A later owner-authorized restoration is tracked privately.

## Local 0.1.5-dev device test — not published

The owner authorized backup/clean flashing and runtime investigation. The current
0.1.5-dev candidate is installed, retaining Connect Wi-Fi/settings/sessions during
this follow-up upload. See [the update/display evidence](UPDATE_AND_DISPLAY_0.1.5.md).
Normal CA-chain verification, fixed PSRAM buffers, bounded browser requests,
manual Connect updates and Home glyph fixes are implemented. Main-release TLS
manifest lookup and the Connect release API succeeded on the device in check-only
mode. The owner confirmed the display fix. End-to-end installation, target boot,
rollback, power-loss and wider RF acceptance remain pending. M12 remains partial.
The public prerelease remains 0.1.1; no push or publication occurred.

## Local 0.1.4-dev audit candidate — not flashed or pushed

See [the full audit](DEEP_AUDIT_0.1.4.md). Sixteen public upstream waveforms
exercise 14 of the 20 enabled OOK decoder symbols and 40 native callbacks, with
repeated capture and exact-model preview checks. Unknown models still record.
Host tests, ASan/UBSan, browser checks, isolation audit and pinned build passed.
Non-Atlas preview, long protocol identities, pulse-overflow loss accounting,
SDK Wi-Fi persistence and bounded main-transition handling were corrected.
No device or private backup/capture was accessed. M12 is still partial; all new
hardware behavior and the complete ESP32 OTA path remain unverified. The two
cosmetic requests remain documentation only. Older sections below are historical.

## Previous local 0.1.3-dev repair candidate — flashed for owner testing

Wi-Fi scanning now uses a zero-initialized IDF scan configuration and a 15-second
application deadline, with bounded event notification and explicit cleanup.
The connected device completed a scan with five networks (no names logged).
Setup avoids automatic scans while capture blocks them; driver errors, running
scans and cooldown have distinct diagnostics. USB wifi_scan_start/status expose
only scan state, result count and error, and cannot scan during capture.

Capture files now use /connect/captures/<32-hex-session>/decoded.wxc and
pulses.wxc. Pinned mklittlefs images have a 32-character component limit: host
probes accepted lengths 31/32 and rejected 33, reproducing the old 43/44-character
filename failure. Existing flat sessions remain readable; neither filesystem nor
NVS was erased. Runtime tests exercise actual capture_storage.cpp with filesystem
operations, that filename limit, both streams, recovery/read/delete, rejected
writes and legacy pulse-only sessions. Device auto capture starts successfully.

OOK processing now retains valid edges throughout an active signal even when
the slower cached RSSI sample is low. Early RSSI dips also receive dropout grace;
the old >30ms prerequisite prematurely ended early frames. Frequency, wiring,
OOK profile, enabled decoder set, AGC, bandwidth and RSSI margin are unchanged.
The final flashed build delivered 16 live packets in 91.7 seconds, versus zero
in the earlier bounded baseline. No live-view rejection or capture/decoder/app
queue drops were observed. Automatic preview was still unselected: Atlas/Home
acceptance remains incomplete, and these counts alone do not prove Atlas support.
Library captured_signals/ignored_signals diagnostics reset at the library's
100-signal interval; decoded/live counters are the relevant boot totals.

Checks: bash tools/test-firmware.sh, browser checks via verify_connect_page.py,
python3 tools/check-connect-isolation.py, git diff --check and pinned PlatformIO
build passed. New host tests cover scan completion beyond the old 2.4-second
deadline, initialized config, errors, cleanup, late events and timer rollover,
and OOK edge/dropout policy. A hardcoded 0.1.2 test expectation was corrected to
use firmware_version::display after the version bump exposed it.
RAM 170792/327680; flash 1379889/3342336. Final binary SHA256:
91c33c830b891b3fd9070eb3da236defd3672e5b09025fa125736e39a8a08840.
Owner-authorized staged uploads completed with flash verification; no chip erase,
settings restore, external submission, GitHub push or release occurred. The
previous private original-device backup remains untouched. Serial port closed
after each bounded check. Logs and allowlisted RF summary are in ignored
artifacts/wifi-rf-investigation/. No credentials or native station packets were
printed or copied into source/public documentation. M12 remains partial.

Remaining: confirm decoded models/Atlas preview, sustained RF reception and
recording/download/reboot behavior on hardware, then the owner's Connect-to-main
OTA test and optional full-backup restoration. Cosmetic requests are recorded
in docs/SMALL_UI_TODO.md only.

## Previous local 0.1.2 test candidate — clean-installed, not published

After initially postponing flashing, the owner authorized two verified private
full-flash backups, a chip erase and clean 0.1.2 provisioning. Published 0.1.1
and its assets remain unchanged.
See [the investigation](REVIEW_0.1.2.md) for reproduced failures and test scope.

The Atlas broker now parses immutable native input, so ArduinoJson cannot destroy
the packet before preview ingestion. Automatic preview waits for three recognized
packets and retains its identity when neighbors appear. Explicit null values are
unavailable, 360-degree direction is accepted and negative wind is rejected.
Browser Start creates a bounded device-owned recording; browser closure does not
end it. Shared startup readiness, actionable capture/storage messages and fully
initialized storage readiness replace generic refusals and false-ready states.
Stop paths retain real stop/flush-failure reasons. No format or data migration
was introduced.

Final SC05_X clean build passed in 30.62 seconds: 167,864 / 327,680 RAM bytes,
1,377,065 / 3,342,336 app flash bytes. The application image is 1,377,424 bytes,
SHA-256 `3dc8b47caa31b1470e77295a8ed87807456288b5d26a6b0d28de19fae753a4e8`.
The full host suite, synthetic browser suite, preview AddressSanitizer/UBSan runs,
compiled runtime/stack/RAM checks, isolation, capture order and decoder audits
passed. Largest main frame: 6,576 bytes; loop override: 24 KiB; internal static
symbols: 157,235 bytes within the 175 KiB guard. This is software evidence only;
Subsequent initial boot checks confirmed 0.1.2, AP/mDNS, display/radio initialization,
ready capture storage and zero retained sessions. No live packets arrived during
that bounded check; RF and OTA acceptance remain pending. M12 stays partial.

Correction to earlier storage descriptions: the existing 256 KiB cap is per
stream per session, not a shared global pool. Eight retained sessions and a
512 KiB filesystem reserve remain unchanged. Browser-started sessions now have
the same one-hour limit as automatic sessions; USB streaming retains its lease.

## Published 0.1.1 software prerelease

Version 0.1.1 is an experimental, owner-authorized software prerelease for
initial device testing. It is not hardware-accepted and is not a validated
production release. The release is [available as a GitHub prerelease](https://github.com/sideout909/WX-Link-Connect/releases/tag/v0.1.1).
Milestone 12 remains partially complete until the hardware checks below pass.

## Changes in 0.1.1

- Restore `http://wxlink.local` with mDNS and setup-network DNS; retain IP fallback.
- Move the fixed decoder-message/formatter pools to checked PSRAM allocations,
  freeing internal RAM for Wi-Fi/HTTP. A compiled RAM budget guards against regression.
- Discover up to 16 nearby 2.4 GHz Wi-Fi names asynchronously; preserve manual
  hidden-network entry and support explicitly clearing a saved password for open Wi-Fi.
- Save Wi-Fi/admin/display settings together before scheduling a restart after
  2.5 seconds. Reject scan/save during capture and refuse new mutations during restart.
- Start one device-owned, bounded session on the first home-Wi-Fi connection per
  boot; it survives browser closure and Wi-Fi outage, ends at one hour or a storage
  limit, and does not restart automatically after Stop/failure. Each decoded/pulse
  stream has a 256 KiB budget; at most eight sessions are retained, with no automatic deletion.
- Always show a Home decoded count and protected latest-live-native-JSON browser
  view, including unadapted devices, independently of recording/preview selection.
- Add host lifecycle tests and browser coverage for hostile/empty/failed scans,
  hidden/open networks, failed persistence/restart feedback, automatic Stop and
  native packet display. Synthetic tests remain separate from live evidence.

## Software evidence

The tested build command is:

```bash
bash tools/pio.sh run -d firmware
```

It targets PanelLan ZX2D80CE02S-V10 / SC05_X, ESP32-S3 N8R2, with
`espressif32@6.12.0`, Arduino-ESP32 2.0.17, LovyanGFX 1.2.7, ArduinoJson
6.21.5, PanelLan 0.0.5, RadioLib 7.7.1 and `rtl_433_ESP` 0.6.3. The last
software build used 167,864 of 327,680 reported RAM bytes (51.2%) and 1,375,165
of 3,342,336 application flash bytes (41.1%). The application image is
1,375,536 bytes; its SHA-256 is recorded in the release manifest and checksums.

The following host checks passed:

- `bash tools/test-firmware.sh` — C++ model, callback-bound, storage, security,
  collector/privacy, adapter and synthetic Connect-page tests.
- `python3 tools/check-connect-isolation.py` — Connect-only settings/API and
  compiled exporter/updater isolation.
- `python3 tools/check-connect-runtime.py` — compiler stack-frame budgets for
  the receive loop and fixed-stack transition worker. Loaded stack high-water under RF/HTTP stress remains unmeasured.
- `python3 tools/check-capture-order.py` — capture precedes station/preview
  filters and retains native decoded output.
- `python3 tools/check-decoder-profile.py` — 20 unique registry symbols match
  the pinned 433.92 MHz OOK profile.
- Optional `tools/verify_connect_page.py` Playwright browser run — passed against
  synthetic APIs, including deliberate main-project transition confirmation.
  Playwright and its browser are separate optional test dependencies; the normal
  firmware setup and host suite do not install them.

The build and host checks do not validate radio reception, electrical margins,
device behavior or station compatibility. The only built profile requests
433.92 MHz OOK with one CC1101. Decoder inclusion is not hardware verification;
profile switching and FSK remain deferred.

## Isolation and transition

Connect stores its own settings in the Preferences namespace `wxconnect` and
capture files under `/connect/`. It does not import the existing weather
firmware's credentials or migrate its settings. The production observation
exporter and generic updater source files are excluded from the release build;
there are no background update checks. The only OTA action is a protected,
explicitly confirmed, one-way transition to the fixed `sideout909/WX-Link`
repository. It copies no Connect settings and uploads no captures.

## Remaining hardware acceptance

- Confirm board/radio wiring, voltage levels and loaded 3.3 V supply current.
- Measure task stack high-water and free heap while display, HTTP, USB and RF run.
- Verify receiver initialization and applied radio settings on the device.
- Compare known, repeated, neighboring and unknown packets against a reference
  receiver; measure truncation, callback and queue loss under live RF.
- Verify selected-device Home readings and browser/touch behavior on the display.
- Exercise storage exhaustion, retained-session deletion/export, restart recovery,
  Wi-Fi outage and interrupted downloads.
- Test Connect-to-main download, installation, rollback, power interruption,
  settings/filesystem behavior and first-boot setup on hardware.
- Run and document an RF-loaded soak test.

Initial USB flashing is owner-authorized. The 0.1.0 repair boot showed an active
AP, mDNS, initialized display/radio, 73–76 KiB free heap and one associated client.
This is limited startup evidence, not RF or hardware acceptance. The flashed
0.1.1 image reported home Wi-Fi connected, mDNS active, device-owned recording
active, initialized display/radio and three live decoded packets. Initial free
heap was approximately 63 KiB. These packets were not compared with a reference
receiver and establish no station compatibility claim. The release manifest
records the exact flashed image and bounded startup checks. Wi-Fi discovery/save/restart browser
flows passed synthetic tests; full end-to-end phone acceptance remains pending.
The local idea list in [WISHLIST.md](WISHLIST.md) includes a possible future
owner-operated submission relay; there is no submission service or credential
on the device in this release.

Additional Atlas preview cause: the live library appends a 95-character public
decoder name in protocol. Both CandidateTable and preview identities previously
accepted only 39 characters and rejected the entire identity. Protocol identity
storage now permits 127 characters, separately from the other bounded fields.
A synthetic regression uses the exact vendored public decoder name and verifies
automatic candidate selection plus temperature ingestion. Recording remains
unchanged. Native stored evidence was inspected in memory only: 26 recovered
packets identified as Acurite-Atlas; only model counts and field names were
printed, never device IDs, measurements, credentials or full packets. Six retained
sessions were present during that check; none were deleted.

Final device verification after the protocol-capacity fix: 0.1.3-dev delivered
three live decoded packets and automatically selected preview by uptime
11,987 ms, with zero live-view rejection and zero capture/decoder/app queue
drops. Hardware LCD values were not visually checked. The earlier unselected
preview observation above applies to the intermediate build and is superseded
by this result. Summary: artifacts/wifi-rf-investigation/atlas-final-summary.json.

Repeated diagnostic resets filled all eight retained-session slots. Removed
only four positively identified empty diagnostic sessions: each contained just
one 0.1.3-dev session-header record and zero pulse records, with no packet data.
All sessions containing evidence were preserved. Final bounded status confirmed
active recording, storage ready and five retained sessions; USB closed afterward.
The original private full-flash backup remains unchanged.

Resume: software fixes and owner-authorized test flashing finished; do not
implement the two cosmetic requests yet (docs/SMALL_UI_TODO.md). Device runs
0.1.3-dev with Wi-Fi scan, capture filename, OOK processing and long protocol
identity fixes. Sustained/visual RF acceptance and Connect-to-main OTA/restore
are still pending; M12 remains partial. No publication/push performed.
