# Independent review repair follow-up — October 22, 2026

The reviewed local candidate had SHA-256
`6f8fb624ee1ac57eeb76880fe690457b3cbf7ac05ce140241bb04f6440479c94`. The five
runtime/evidence findings in `INDEPENDENT_REVIEW_2026-10-22.md` were traced into
the firmware and collector and addressed in the current source.

## Repairs

- Replaced automatic `Record` and `PulseFrame` temporaries in the capture drain
  path with owned loop-task scratch objects. Added compiler `.su` reports and
  `tools/check-connect-runtime.py` to enforce frame budgets in receive, capture,
  command, and web paths. The reviewed source already defined a 24 KiB
  `SET_LOOP_TASK_STACK_SIZE` override, and its ELF had a strong
  `getArduinoLoopTaskStackSize()` symbol; this conflicts with the review report's
  statement that no override was present and the task therefore used the 8 KiB
  SDK default. The buffers were still moved and frame budgets are now checked.
- Changed `radio_receiver::pop` to accept an exact-size array reference. The live
  drain passes that array directly, so `sizeof(pointer)` cannot be accepted as
  its capacity.
- Added owner-specific USB/browser capture leases. Browser start selects the
  browser owner, and the authenticated, CSRF/origin-protected heartbeat route
  renews only that owner's lease. The page sends a heartbeat every five seconds;
  USB heartbeats cannot extend browser sessions and status reads do not renew
  either lease.
- Made storage append rejection terminal for the active session. The writer
  counts the failed and queued records, aborts the session, closes storage, and
  reports `storage_write_failed`; timed flush failure follows the same terminal
  path and reports `storage_flush_failed`. Stream capacity latches storage
  failure after preserving its accepted buffered prefix.
- Stored capture loss counters and stop reason on successful end records, and
  firmware/board identity on the session-start record. Historical export now
  reads those stored records and never queries current `capture_status` for a
  selected session's totals. Older or interrupted sessions without stored
  diagnostics export `null` for unknown historical counters and build identity.

## Verification

- `bash tools/pio.sh run -d firmware` — passed for the pinned SC05_X / ESP32-S3
  profile. Linker usage: 243,556 / 327,680 bytes RAM and 1,113,153 / 3,342,336
  bytes flash; app image: 1,113,520 bytes.
- `bash tools/test-firmware.sh` — passed capture, writer-failure, lease,
  truncation, storage-format, preview, security, and inherited host checks; 9
  inherited console, 16 collector/privacy, and 4 Connect-page tests passed.
- `python3 tools/check-connect-runtime.py` — passed compiled frame budgets and
  runtime wiring; largest `main.cpp` frame was 6,560 bytes, `loop()` frame was
  224 bytes, and the compiled ELF includes the strong 24,576-byte stack override.
- `python3 tools/check-connect-isolation.py`,
  `python3 tools/check-capture-order.py`, `python3 tools/check-decoder-profile.py`,
  and `python3 tools/check-handoff.py` — passed.
- `/home/lschaar/.local/share/cyd-weather-link/venv/bin/python
  tools/verify_connect_page.py` — passed with synthetic APIs and a local browser.
- Collector regression tests exported session A while session B and different
  current firmware metadata were present; the bundle retained A's counters and
  build identity. A pre-repair session without those records produced `null`,
  not zero or the current session's values.

No device, RF receiver, USB port, or serial console was accessed. These software
checks do not establish RF reception, stack high-water use, storage behavior on
physical flash, or hardware acceptance. The replacement candidate remains a
local experimental application image, not a validated release.
