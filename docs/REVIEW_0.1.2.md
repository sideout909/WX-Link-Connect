# 0.1.2 Atlas preview and capture-start investigation

This is a local test candidate. The owner initially postponed flashing, then
authorized verified full-flash backups, a chip erase and a clean 0.1.2 install.
Initial startup and empty capture storage are verified; no full device/RF
acceptance or publication is claimed.
The connected device reported main WX-Link 0.7.5 before the no-device-access hold;
only allowlisted status fields were read. No credentials, NVS or packet payloads
were read from it. No subsequent port access is required to reproduce these bugs.

## Proven Atlas preview failures

In 0.1.1, `liveReceive()` passed its mutable `char*` packet into
`deserializeJson(d,packet)`. ArduinoJson 6 chooses zero-copy parsing and changes
that packet in place. Capture and the latest-native view had already copied the
original text, so those could appear healthy while the subsequent
`preview_adapter.ingest(packet,...)` reparsed damaged JSON and failed. Earlier
adapter tests passed standalone const strings and never exercised this broker
parse followed by preview parsing.

The new `connect_preview_pipeline_test.cpp` reproduces the old mutation and failed
adapter ingestion using the pinned ArduinoJson. It then exercises the production
`connect_preview.h` const-input parser, candidate selection, adapter and capture
queue with synthetic native Atlas packets matching the vendored decoder schema.
All seven preview fields become valid across alternating temperature/humidity,
wind/rain and light/UV packets. Original native JSON stays byte-identical in the
recording queue; repeated values retain independent field receive times.

The second failure was `soleCandidate()`: any second identity caused automatic
preview to clear. Auto-selection now waits for three packets from a recognized
weather model with an ID, then retains that full identity. Unknown neighboring
identities neither block Atlas selection nor erase it. Explicit selection still
wins, and preview selection does not restrict recording. The native Atlas decoder
omits `protocol`; that is supported. An initial suspicion that protocol presence
was required was checked and rejected as a cause.

Additional adapter fixes distinguish an omitted field from explicit `null`, accept
the decoder's valid 360-degree direction, and reject negative wind speeds. Missing
fields keep their own previous timestamps; explicit null becomes unavailable.
Home also shows the selected channel alongside model/ID. No unknown field or
source numeric precision is normalized in stored native packets.

## Capture and status corrections

The exact owner's 0.1.1 Start failure cannot be reconstructed from the currently
running main firmware; no claim is made that every observed refusal was a storage
failure. Source inspection established these independent weaknesses:

- Start was disabled both when an automatic session was already running and when
  storage was unavailable, with little explanation. Status and UI now distinguish
  already recording, missing radio, Wi-Fi scan, restart/transition, unavailable
  storage, eight retained sessions and the free-space reserve. Storage's last
  operation message is displayed as text. File-open/start failures remain explicit.
- Manual browser Start created a browser lease that expired without heartbeat.
  Browser Start now creates the same device-owned, one-hour-bounded session as
  automatic recording. USB-controlled streaming retains its own heartbeat lease.
  Browser, USB and automatic starts share one readiness/start path; partial starts
  close their new files rather than leave a controller/session half initialized.
- Storage readiness previously meant mounted, even if Connect directory creation
  or recovery initialization failed. It now requires complete initialization.
  No format, migration or deletion of original weather data was added.
- Stop/end paths now preserve their actual reason and report a rejected final
  flush as a storage failure, rather than a normal requested/duration/lease stop.

One documentation correction: existing storage limits are 256 KiB per
**decoded/pulse stream per session**, eight retained sessions, and a 512 KiB
filesystem reserve. They were incorrectly described as one shared 256 KiB pool.
Those existing storage limits are unchanged in this repair.

## Verification and limits

- Full `bash tools/test-firmware.sh` C++/Python suite, including the new broker
  regression, native byte preservation, neighbor/selection tests, null/range
  adapter tests and storage-start boundary policy.
- `tools/verify_connect_page.py` using existing local Playwright: synthetic browser
  tests now cover unavailable storage, retention blockers, failed Start, actual
  device-owned start/Stop feedback, plus existing setup/scan/privacy-safe DOM and
  capture/download/main-transition controls. Older browser heartbeat handling is
  tested separately from new device-owned starts.
- AddressSanitizer/UndefinedBehaviorSanitizer runs of the preview pipeline and
  preview adapter tests pass; these are host tests, not radio fixtures replayed
  onto hardware.
- SC05_X build plus compiled stack/RAM, isolation, capture order and decoder
  profile audits. The capture-order audit now requires the immutable parser;
  its old literal check had accidentally required the defective mutable call.

Exact final build metrics, image hash and commands are recorded in the local
candidate manifest and PROGRESS.md. Existing board/wiring, pinned libraries,
callback bounds, upload isolation, raw-window controls and settings isolation
are preserved. Remaining acceptance: verify 0.1.2 boots, real Atlas measurements
on LCD/browser, recording/start/Stop persistence and duration/storage behavior,
neighbor RF, loaded stack/heap/soak, and OTA transition. M12 remains partial.
