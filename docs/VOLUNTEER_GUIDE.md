# WXLink Connect volunteer guide

WXLink Connect is experimental receive-only firmware for the PanelLan
ZX2D80CE02S-V10 / SC05_X ESP32-S3 N8R2 display with a CC1101. It records
weather-station packets locally so volunteers can review evidence. A decoder
being enabled means its code is in the build; it does not mean a station was
verified on hardware. The current fixed profile requests 433.92 MHz OOK only.
FSK, runtime profile switching, other regions and unsupported bands are not
available. See [the decoder profile](DECODER_PROFILE.md) and [wiring](WIRING.md).

## Build and test

From the project directory, install/use the project-local tool environment and
build the SC05_X firmware:

```bash
bash tools/setup-firmware.sh
bash tools/pio.sh run -d firmware
bash tools/test-firmware.sh
```

No serial port is needed for host tests or compilation. Do not use a full-flash
image or another PanelLan/CYD board profile. The development firmware image is
`firmware/.pio/build/panellan_sc05_x/firmware.bin`; no device should be flashed
as part of host checks. Version 0.1.1 is an experimental software prerelease for
owner-led initial device testing; M12 hardware acceptance remains open. See
[software status](RELEASE_STATUS.md) for exact validation and the release link.

## Prepare the receiver

Use only the SC05_X wiring in [WIRING.md](WIRING.md), a suitable 433 MHz
antenna, and the exact board/radio profile. Confirm pin numbering and voltage
with power disconnected. The display debug 3.3 V pin's available current is
unspecified; check the loaded voltage before relying on it. Keep the radio
receive-only. Do not connect expansion-header 5 V directly to the CC1101.

## Set up Connect

Power the board and join its `WXConnect-…` setup network using the generated
Connect setup key shown by the firmware. Stay connected if your phone warns
that this network has no internet. Open **http://wxlink.local** (HTTP, not HTTPS)
and authenticate as `admin`. If hostname discovery is unavailable on your client,
use **http://192.168.4.1** while connected to the setup network. On home Wi-Fi,
use `http://wxlink.local` or the home-network IP shown on the device. The setup
network remains available; reconnect after a firmware restart. Automatic captive
portal pop-up is not implemented. The admin password is generated/stored in Connect's
isolated settings; choose a new one in Setup if desired. The original weather
firmware's saved Wi-Fi/admin credentials are never imported. Keep the AP key
and admin password private. Configure local Wi-Fi only if needed; capture and
USB collection do not require cloud accounts.

Open Setup to discover nearby 2.4 GHz networks automatically, or use Scan nearby
Wi-Fi to refresh. Choose a network or type a hidden name manually. Scanning does
not choose or connect to a network for you. For an open network, select Clear saved
Wi-Fi password. Save and restart persists Wi-Fi/admin/display settings together,
acknowledges the save, then reboots after 2.5 seconds. A failed save does not reboot.
Stop recording before scanning or saving settings. After reboot, reconnect to
WXConnect or your home Wi-Fi and reopen `http://wxlink.local`.

The Overview shows the selected preview device and received values. Missing
measurements stay unavailable. In Devices, select a preview identity; this
changes the Home preview only. Recording continues for all enabled decoders.
Unknown devices can be recorded even when no weather preview adapter exists.

## Capture and collect

After setup, the first successful home-Wi-Fi connection after each boot starts
one live recording automatically. A browser does not have to remain open, and a
Wi-Fi outage does not end this device-owned session. It stops at one hour or a
storage limit/failure. A stopped or failed session does not restart on reconnect;
use Start capture deliberately or reboot to start another automatic session.
Stop capture works for automatic sessions too. Manual browser and USB sessions
retain their separate heartbeat leases; stop an automatic session before starting
a USB-controlled session.

The Home screen always counts complete live decoded packets. The protected browser
shows the latest original native JSON, including unknown fields and source
precision, independently of recording and preview selection. This latest-packet
view is RAM-only and does not replace a stored capture. Repeated packets increment
the count. Truncated or invalid JSON is rejected and counted rather than presented
as a complete packet. Synthetic/replay inputs do not enter this live RF view.

In Capture, stop the recording when you have enough evidence, then export it. The device
retains up to eight sessions at once in a shared 256 KiB pool; export a session or deliberately delete
one to free a slot. This is a retained-session limit, not a lifetime capture
limit. Optional raw-pulse evidence requires a short, explicit window and is
bounded; pulse records are not raw IQ and are not correlated to decoded packets.

For USB collection, connect one device port and run:

```bash
python tools/connect_capture.py --port PORT --duration 3600 \
  --output wxconnect-captures \
  --station-make "Example" --station-model "Example model" \
  --region "State or broad region" --antenna "433 MHz whip" \
  --supply "USB power" --console-comparison "Compared after capture" \
  --observation-notes "Rain began near the end of the sample"
```

Omit field-note options when unknown. Do not enter a street address, precise
coordinates, account details, passwords or keys. Use `--session-id ID` to
collect an already stored session. The collector validates a complete local
bundle and refuses to overwrite existing output. The browser download is
JSONL evidence; use the USB collector when you need a checksummed bundle.

## Review before sharing

A normal bundle includes packets from all devices the receiver captured,
including neighbors. Unknown native fields may contain identifiers. Validate an
existing bundle offline with:

```bash
python tools/connect_capture.py --validate-bundle PATH
```

Create a separate review derivative with:

```bash
python tools/connect_capture.py --review-bundle PATH
```

Choose one identity or all devices, then choose local-only or manual
public-review intent. The tool requires a typed confirmation and never uploads.
Selected-device derivatives report excluded packet counts and omit pulse data
because no packet-to-pulse correlation exists. You may explicitly create a
second derivative that replaces common ID fields; this changes native JSON in
the derivative and is documented with fresh checksums. The downloaded original
bundle remains unchanged. Common-key redaction cannot guarantee unknown fields
are private. Inspect every output yourself before sharing; a public-review
choice is not a submission or publication.

## Return to WX-Link after station support is added

See the [transition implementation and security contract](CONNECT_TO_MAIN_TRANSITION.md) for its exact checks and limits.

Keep Connect installed while collecting evidence. When the main WX-Link project
publishes a release that includes support for the captured station, export every
Connect session you want to keep and stop recording. Connect has no background
update check. From Setup, choose “Check and switch to WX-Link,” confirm the warning,
and type `SWITCH TO WX-LINK`. The transition requires Home Wi-Fi and valid time; it
downloads only the latest compatible application from the fixed
[`sideout909/WX-Link`](https://github.com/sideout909/WX-Link) repository, then checks
the board, partition layout, newer version, image header and release SHA-256 before
reboot.

This replaces the Connect application but leaves NVS and the Connect capture files
on flash. The main app does not browse those capture files, so export them first.
Connect Wi-Fi/admin settings are not copied into WX-Link settings; on a newly shipped
device, reconnect to WX-Link's setup network and configure it after reboot. The
transition does not upload captures or credentials. HTTPS and the manifest hash
protect transport/integrity; the release has no separate publisher signature.

## Current limits

- No station compatibility, RF sensitivity, hardware profile application or
  loaded supply behavior is confirmed by the host build.
- The registry has 20 reviewed OOK decoder symbols. All remain unverified on
  hardware; source inventory and compilation are not station validation.
- Profile initialization can be accepted by the pinned library while register
  readback remains unverified. There is one fixed OOK profile; profile switching
  and FSK are deferred.
- Raw pulses are bounded envelope timings, not IQ samples. Callback and queue
  limits can drop data; review the reported loss totals.
- Captured device IDs and timing can be sensitive. No cloud, weather-service,
  generic/automatic updater, or automatic upload path is provided. The explicit
  one-way Connect-to-WX-Link transition is described above.

Use the [issue template](ISSUE_TEMPLATE.md) to prepare a report. Never attach a
full-flash image, NVS dump, credentials, owner logs, or an unreviewed capture.
