# Independent candidate review — October 22, 2026

## Result

Not ready for flashing or volunteer testing. Five actionable runtime/evidence
findings remain despite passing current checks. This review changed no firmware,
accessed no hardware, and did not replace the candidate. Original weather project
files were not changed. M1 isolation is supported by the source and build; the
claims that all later software milestones are complete need revision.

Reviewed candidate SHA-256:
`6f8fb624ee1ac57eeb76880fe690457b3cbf7ac05ce140241bb04f6440479c94`.

## P0 — compiled task stack exceeds its allocation

Location: firmware/src/main.cpp:1214 and :603.

The pinned Arduino ESP32-S3 configuration allocates 8,192 bytes to loopTask.
No firmware stack-size override exists. Disassembly of the actual compiled ELF
shows the following unconditional function-entry stack frames:

| Function | Frame bytes |
| --- | ---: |
| loop() | 9,984 |
| liveReceive() | 6,560 |
| drainPulseQueue() | 10,464 |
| drainCaptureQueue() | 6,352 |

The loop frame alone exceeds the task stack, before any nested calls. The large
local Record and PulseFrame objects make nested paths worse; liveReceive calls
both drains on every loop, including when no pulse window is open. This creates
a stack-corruption/reset risk on normal startup, not merely maximum RF load.

Fix: remove large automatic records/frames and inspect optimized call-chain
stack use, including web handlers and USB dispatch. Use owned reusable buffers
with explicit lifetime/reentrancy constraints or a measured task allocation;
raising the stack to cover one frame alone is insufficient. Add a compiled-stack
budget check and then measure stack high-water/heap on authorized hardware.

Evidence command: Xtensa objdump -d -C on
firmware/.pio/build/panellan_sc05_x/firmware.elf; inspect each entry instruction.
SDK source: framework-arduinoespressif32/tools/sdk/esp32s3/qio_qspi/include/sdkconfig.h
sets CONFIG_ARDUINO_LOOP_STACK_SIZE to 8192; cores/esp32/main.cpp uses it.

## P1 — receive drain always rejects the buffer

Location: firmware/src/main.cpp:1121–1124; firmware/src/radio_receiver.cpp:66–67.

liveReceive aliases the 16,384-byte array as `char* packet`, then calls pop with
`sizeof(packet)`. This is a four-byte pointer on ESP32-S3. pop rejects capacities
below 16,384 before touching its queue. Consequently live callback records never
reach capture, candidate discovery or preview, and the receiver pool fills.

Fix: pass sizeof(radio_packet_copy), or use an array-reference interface that
cannot accept pointer size by mistake. Add an integration test that drives the
real drain boundary with a queued native packet and observes capture plus preview;
substring ordering audits do not exercise this behavior.

## P1 — browser recordings expire after 20 seconds

Location: firmware/src/main.cpp:1016, :1218; firmware/include/setup_page.h:26.

Both USB and HTTP capture start use the same 20-second lease timestamp. Only USB
capture_keepalive updates it after start. The browser merely polls status every
three seconds; the status route does not renew the lease, and no HTTP heartbeat
route is registered. A browser-only capture therefore stops after about 20 seconds
even while the page remains open. The mocked browser test stops immediately and
does not simulate the firmware's lease clock.

Fix: define ownership/lifetime semantics explicitly. Either expose an authenticated,
CSRF-protected browser heartbeat and send it, or make browser capture independent
of the USB lease. Test more than 20 seconds with a simulated clock, as well as
page closure and USB/browser coexistence. Avoid letting unrelated reads silently
extend a state-changing lease.

## P1 — storage capacity/failure does not stop recording

Location: firmware/src/capture_storage.cpp:235–256;
firmware/src/main.cpp:596–601 and :1228.

append returns false at a stream budget; writeCaptureRecord only increments a
counter. It never closes the session or changes recording status. A failed timed
flush also increments a counter without closing capture. The UI can continue to
say RECORDING while new evidence is discarded indefinitely. At capacity an end
record can also fail, yet finish can report a successful flush because previously
buffered bytes flushed successfully. Successful flush is not proof the final
record was accepted.

Fix: distinguish a deliberate bounded raw-window drop from a terminal decoded
capacity/flash failure. Stop/close on terminal failure, expose the reason, and
reserve final-status capacity or persist an explicit incomplete-session state.
Propagate append/end-record failure to stop acknowledgments. Add storage-layer
integration tests for full streams, reserve exhaustion, failed writes and final
acknowledgments; framing-only tests do not cover this state machine.

## P1 — saved-session export attaches another session's diagnostics

Location: tools/connect_capture.py:268–299; firmware/include/capture_jsonl.h:125;
firmware/src/main.cpp:692–702.

export_bundle queries the current in-memory capture_status and uses those values
as the selected stored session's loss totals, without checking its session_id.
Export session A after recording B, or export A after a reboot, and its counters
come from B or reset memory. Firmware/board provenance also comes from the current
running firmware. Stored session/end records do not include the loss counters,
and queue losses before sequence assignment cannot be reconstructed from gaps.
The validator checks the generated manifest against the same incorrect diagnostic
snapshot, so self-consistency does not establish historical accuracy.

Fix: persist per-session capture counters, receiver/raw loss deltas and firmware
identity; use those for historical exports. Unknown recovery statistics must be
labeled unavailable, not replaced with zero. Add A-then-B and reboot/export tests.
Do not silently present current firmware identity as the recording build.

## Checks independently rerun

- bash tools/pio.sh run -d firmware: PASS. RAM 232,996 / 327,680;
  flash 1,104,437 / 3,342,336. Candidate binary exactly matches current build.
- bash tools/test-firmware.sh: PASS, including 14 collector/privacy,
  3 page and 9 inherited console tests, plus host C++ checks.
- check-connect-isolation.py, check-capture-order.py,
  check-decoder-profile.py, check-handoff.py: PASS.
- verify_connect_page.py with the existing tool venv: PASS against synthetic APIs.
- Candidate byte count/hash: PASS, 1,104,800 bytes; ZIP CRC and binary match: PASS.

The checks passing alongside the findings demonstrate missing runtime integration
coverage. The capture-order and isolation scripts inspect source text and selected
ELF strings; they are useful guards, not proofs of full runtime behavior.
The handoff checker still prints its historical statement that capture firmware
is not implemented; its purpose remains checking handoff structure only.

## Isolation findings supported by source

services.cpp opens Preferences namespace wxconnect and loads an explicit Connect
allowlist. platformio.ini excludes network_jobs.cpp and firmware_updates.cpp.
Connect web handlers require local admin authentication; state changes check CSRF
and supplied Origin. Legacy routes fall back to 404. These checks support the
corrected milestone 1 claims, unlike the earlier repository-rename explanation.

## Suggested repair order

1. Fix stack use and the receive-buffer capacity error before any flashing.
2. Fix browser lease ownership and terminal storage failure behavior.
3. Persist session diagnostics/provenance and test historical export.
4. Rebuild, rerun meaningful integration tests, replace candidate/manifest/ZIP,
   and update milestone evidence. Then perform separately authorized hardware
   acceptance; enabled decoders remain unverified physical compatibility.
