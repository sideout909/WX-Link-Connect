# Start here

WXLink Connect is a separate experimental packet-capture firmware project. The current software implementation, exact checks and hardware limitations are recorded in [the software status](docs/RELEASE_STATUS.md). Read [the volunteer guide](docs/VOLUNTEER_GUIDE.md), [scope](docs/01_SCOPE_AND_ARCHITECTURE.md), [capture contracts](docs/03_CAPTURE_AND_HOME_CONTRACTS.md), and [the Connect decoder profile](docs/DECODER_PROFILE.md) before building or collecting evidence.

Build for the preserved PanelLan SC05_X profile and run host checks:

```bash
bash tools/setup-firmware.sh
bash tools/pio.sh run -d firmware
bash tools/test-firmware.sh
```

No serial port is needed for these commands. Do not flash a device as part of these software checks. The current source is a local 0.1.2 test candidate, not a hardware-accepted release. Published prerelease 0.1.1 remains unchanged. See [the 0.1.2 investigation](docs/REVIEW_0.1.2.md) for the Atlas/capture fixes and their software evidence. Hardware and RF acceptance remain pending.

The first-run admin credential is created in Connect's isolated settings. Do not import or reuse the original weather firmware's saved password. Never include NVS, credentials, full-flash images, owner logs or unreviewed captures in a bundle.

After the main WX-Link release includes support for a captured station, volunteers
can export needed sessions and use Setup → “Check and switch to WX-Link.” This is
the only OTA action in Connect; it targets the fixed `sideout909/WX-Link` repository
and requires typed confirmation. Connect settings are not copied. On a new device,
reconnect to WX-Link setup after reboot. See the [volunteer guide](docs/VOLUNTEER_GUIDE.md).

For the transition API, integrity checks, retained data behavior and test limits, see [Connect → main WX-Link](docs/CONNECT_TO_MAIN_TRANSITION.md).
