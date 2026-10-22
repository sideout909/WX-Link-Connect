# Connect decoder profile

WXLink Connect currently builds one reviewed receiver profile:

- Center frequency: 433.92 MHz
- Modulation: OOK
- Decoder library: `rtl_433_ESP`, pinned to `f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8`
- Registry: the 20 symbols recorded in `firmware/include/connect_decoder_profile.h`
- Profile ID: `wx-ook43392-rtl433-f0a8156-v1`

The registry is a reviewed software candidate set, not a list of stations tested
on hardware. Symbols are checked against the local library inventory and build
configuration by `python3 tools/check-decoder-profile.py`. That check verifies
the selected symbols and modulation metadata; it does not prove RF compatibility.

One CC1101 receives one profile at a time. Profile switching and FSK are
deferred. Connect does not enable every decoder in the upstream library, scan
multiple bands, or claim simultaneous support for multiple modulation modes.
See [software status](RELEASE_STATUS.md) for pending device and RF acceptance checks.
