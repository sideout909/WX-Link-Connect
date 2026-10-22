# Public rtl_433 replay fixtures

These pulse measurements derive from publicly shared captures in
[merbanan/rtl_433_tests](https://github.com/merbanan/rtl_433_tests).
They are external historical data, not newly collected Connect live evidence.
See `provenance.json` for pinned source paths, hashes and extraction instructions.
Only pulse/gap numbers and frame boundaries are retained from extracted `.ook`
files. `.expected.json` contains the independent upstream decoded packet facts.
The original IQ stays in the upstream repository and ignored local research files.

Run `python3 tools/test-public-decoders.py` from the project root. It compiles
all 20 enabled vendored decoders, replays 16 waveforms offline, compares 40 native
outputs with upstream references, then exercises Connect capture and preview.
No SDR, USB port, network, desktop rtl_433 installation or device is used by the test.
Use `--sanitize` for ASan/UBSan. The integration test labels its captures `replay`.

This verifies post-demodulator software; hardware/RF compatibility is not inferred.
See [the audit](../../../../docs/DEEP_AUDIT_0.1.4.md) for coverage and limitations.
