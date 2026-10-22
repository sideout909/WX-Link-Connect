# WXLink Connect 0.1.1 software status

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
  limit, and does not restart automatically after Stop/failure. Shared capture pool
  stays 256 KiB, with at most eight retained sessions and no automatic deletion.
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
