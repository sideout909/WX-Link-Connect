# WXLink Connect

WXLink Connect is experimental receive-only capture firmware for the PanelLan
ZX2D80CE02S-V10 / SC05_X ESP32-S3 and CC1101. It records native decoder packets
and bounded optional pulse timings locally, shows a selected-device weather
preview on a familiar Home screen, and supports protected browser controls and
USB collection. The name reflects its purpose: connect more people to the
WX-Link project by giving volunteers a practical way to contribute data from
weather stations that the main firmware does not yet support. It is a separate
product with Connect-only settings and no weather-service uploader, generic
updater or background OTA checks. After a
main WX-Link release adds a volunteer's station, a protected, explicitly confirmed
action switches Connect one-way to the latest compatible release at
[`sideout909/WX-Link`](https://github.com/sideout909/WX-Link). Export captures first;
the main app does not browse Connect sessions or import Connect settings.

## Build and use

Start with [the volunteer guide](docs/VOLUNTEER_GUIDE.md),
[SC05_X wiring](docs/WIRING.md), and [capture contracts](docs/03_CAPTURE_AND_HOME_CONTRACTS.md).
Build and run host tests with:

```bash
bash tools/setup-firmware.sh
bash tools/pio.sh run -d firmware
bash tools/test-firmware.sh
```

The active profile is a single requested 433.92 MHz OOK profile. The 20 enabled
decoder symbols and pinned source revision are in the [Connect decoder profile](docs/DECODER_PROFILE.md).
Enabled means compiled into the registry; no listed station is hardware-verified.
Profile switching and FSK are deferred. A successful host build does not prove RF
reception, loaded power behavior, station compatibility, or browser behavior on hardware.

Setup discovers nearby 2.4 GHz Wi-Fi networks and saves settings with an automatic
restart. After home Wi-Fi connects, a bounded recording starts automatically; it
continues without an open browser. Home counts live decoded packets, and the
protected browser shows the latest native packet independently of recording.

Capture sessions are bounded and retained locally. The USB collector creates a
checksummed bundle; `python tools/connect_capture.py --validate-bundle PATH`
validates it and `--review-bundle PATH` creates a separate privacy-reviewed
derivative. Captures may contain nearby device IDs and timing. Review every
bundle manually; no automatic submission exists.

See [software status](docs/RELEASE_STATUS.md) for exact checks, build size and remaining
hardware acceptance tasks, and the [wishlist](docs/WISHLIST.md) for future ideas.
The current source builds a local **0.1.2 test candidate** with Atlas preview and
capture-start fixes; see [the investigation](docs/REVIEW_0.1.2.md). It has been clean-installed for initial owner testing and has not been
published. Version 0.1.1 remains the published experimental software prerelease
for initial owner-led device testing. It has not passed hardware/RF acceptance.
Host checks do not validate radio performance or station compatibility.

The project retains GPL-3.0-only and third-party notices; see [LICENSE](LICENSE)
and [third-party notices](third_party/README.md). Historical weather-project
handoff notes and unrelated PC capture tools are kept out of the public source
tree.

Transition security and recovery behavior is documented in [Connect → main WX-Link](docs/CONNECT_TO_MAIN_TRANSITION.md).
