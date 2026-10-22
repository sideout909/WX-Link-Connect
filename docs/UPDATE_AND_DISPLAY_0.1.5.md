# Update and display investigation — 0.1.5-dev

The owner reported a -12288 TLS certificate error, a frozen update page, missing
Connect self-updates, a gray initial browser Wi-Fi badge, and corrupted degree/
separator glyphs on the LCD. The current local candidate is installed on the
owner's ESP32-S3. No GitHub push or new release publication occurred. M12 remains
partial; release-manifest checks are not completed firmware installations.

## Findings and implementation

- Three fixed loop-owned packet/export buffers occupied 45562 bytes of internal
  RAM. They now live in one bounded, startup-allocated PSRAM pool. No callback
  allocates, writes storage or runs HTTP. The typed radio buffer remains an array,
  preserving its true capacity. Allocation failure fails startup explicitly.
- The pinned SDK requires internal memory for TLS. The normal CA-chain verifier
  now receives three trusted public CA roots explicitly, including the resolved-IP
  overload that also takes the original hostname. Hostname, certificate signature
  and validity checks remain required. No insecure fallback or leaf pin was added.
  A low-memory guard fails the request cleanly before starting TLS.
- The previous SDK compact-bundle callback collapses several failures into -12288.
  See [the primary SDK report](https://github.com/espressif/arduino-esp32/issues/8626)
  and [CA provenance](../firmware/certs/README.md). This investigation does not prove
  whether the original failure was a cross-signed-chain corner case or allocation
  pressure. The revised normal verifier has now passed on the actual device.
- Browser requests now have an eight-second deadline covering headers and body;
  status polls and latest-packet polls cannot overlap themselves. Latest-packet
  polling pauses during firmware work. A lost connection reports uncertainty and
  asks the user to leave power on and reconnect rather than claiming success.
- A repeated device check exposed an intermittent Connect API response timeout.
  Unknown-length JSON bodies now recognize a complete bounded object/array instead
  of waiting for a delayed socket close. Quotes/escapes are tracked, depth is capped
  at 32, oversized/incomplete data fails, and ArduinoJson still validates syntax
  and policy. Total/idle deadlines remain 15/3 seconds. Read failures report length
  and result information. Three consecutive final API checks passed afterward.
- **Update Connect from GitHub** is a separate protected/manual action. It reads
  the fixed Connect repository's five latest published releases, ignores drafts
  and nonnumeric tags, includes numeric experimental prereleases, and rejects
  older/same Connect releases (a matching numeric release can replace its `-dev`
  build). Product/repository/version, board, partition, flash size, image header,
  byte count and SHA256 are checked before activation. No credentials are needed.
  Main WX-Link version numbers remain independent from Connect's versions.
- Connect updates require a new schema-1 OTA manifest and versioned application
  asset. [The transition guide](CONNECT_TO_MAIN_TRANSITION.md) documents exact names
  and `tools/package-connect-update.py`. The packager never publishes and refuses
  a stale/wrong-version or non-ESP32-S3 image. Public v0.1.1 predates this OTA
  manifest contract and is older than the installed local candidate.
- The browser's initial **Setup Wi-Fi** badge is orange, including its border.
  It returns to the normal badge style after Home Wi-Fi connects. This fixes the
  browser separately from the previously changed physical-screen indicator.
- The physical screenshot showed Font0 extended-glyph mismatches at the station
  identity, temperature, wind and stopped-state labels. Home separators are now
  ASCII and actual degree marks are drawn as circles. Before/after LCD screenshots
  were inspected locally; the owner also confirmed the fix. Packet source JSON is
  unchanged. Private screenshots are not repository assets.

## Verification

Passed commands (no dependency upgrades):

```bash
bash tools/test-firmware.sh
/home/lschaar/.local/share/cyd-weather-link/venv/bin/python tools/verify_connect_page.py
python3 tools/check-connect-isolation.py
python3 tools/check-connect-runtime.py
git diff --check
bash tools/pio.sh run -d firmware -t upload --upload-port /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_68:B6:B3:34:82:48-if00
```

The full host suite includes existing capture/preview and 16 public-waveform
replays, Connect release selection and manifest identity, downgrade/wrong-repository
rejection, fragmented/oversized/deadline-bound JSON transfer, offline public TLS
chain/hostname checks, and package identity/header checks. Synthetic browser checks
cover the initial computed orange color, removal after connection, both update
controls, and a genuinely hung fetch that times out without overlapping polls.
Offline TLS fixtures are public GitHub chains observed on 2026-10-22; tests use
that date to keep preserved leaf expiry separate from current network checks.

Final build RAM: 127568/327680; flash: 1337993/3342336.
Application: 1338352 bytes; SHA256:
`d7d00595dad87860791855403034121a0139ffc499058b4b9bbce39dc270129e`.
Compiler stack budget checks pass; worker/main call-chain hardware stress is not
established by individual .su frames.

The final application upload succeeded with hash verification and retained the
Connect settings and capture storage. No second full-chip erase occurred. The
private full 8 MiB backup from before the original clean installation remains
in the ignored backup directory; it is not part of this repository or a bundle.

USB tests used board-specific DTR/RTS asserted together. An earlier ad hoc status
check incorrectly deasserted them and reset the native USB board; subsequent
checks and screenshots use the existing console's known control-line behavior.
No reset command was sent during the final lookup checks.

On the final installed binary, check-only USB operations verified:

- Main target: real HTTPS release manifest accepted, progress 100, install=false,
  restart_pending=false. This exercised GitHub and its release-asset redirect.
- Connect target: three successive real release-API lookups returned the expected
  "No newer Connect release published" result, without changing firmware.
- Display, radio and storage initialized; saved Wi-Fi reconnected; live packets
  still reached the selected preview. Six sessions were retained after the test
  reboots; none were deleted. Recording was stopped and flushed for update checks
  and remains stopped, ready for the owner's next update attempt.
- Reported post-check free heap was about 109 KiB, with a largest internal block
  around 49 KiB and loop stack headroom 10564 bytes. These are bounded observations,
  not long-term stress or all-radio compatibility acceptance.

Evidence and private screenshots: `artifacts/tls-investigation/`; final repeatable
check script: `artifacts/tls-investigation/check-device.py`. It only stops/flushes
recording and queues check-only lookups, never image installation. The serial
port was closed after the checks.

## Remaining acceptance

The owner must still exercise a complete main WX-Link installation/first boot,
settings/capture retention, interruption recovery and rollback. Connect self-OTA
needs a newer numeric release published with the new manifest contract before an
actual self-upgrade can be validated. No publication is implied by local tests.
Wider RF/device acceptance and worker stack/heap stress remain pending. The prior
20-decoder inventory and public-waveform evidence remain valid with their stated
hardware limits; this task does not establish additional station compatibility.
