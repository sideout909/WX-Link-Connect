#!/usr/bin/env python3
"""Verify the enabled firmware registry matches the reviewed 433 MHz OOK list."""
import csv
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
devices = (root / "firmware/include/connect_devices.h").read_text()
profile = (root / "firmware/include/connect_decoder_profile.h").read_text()
ini = (root / "firmware/platformio.ini").read_text()
rows = {}
with (root / "reference/decoder-inventory/433-weather.csv").open(newline="") as stream:
    for row in csv.DictReader(stream):
        rows[row["symbol"]] = row

registry_block = devices.split("#define DEVICES", 1)[1].split("#define NUMOFDEVICES", 1)[0]
registry = re.findall(r"DECL\(([^)]+)\)", registry_block)
profile_block = profile.split("kEnabledDecoderSymbols[] = {", 1)[1].split("};", 1)[0]
recorded = re.findall(r'"([a-zA-Z0-9_]+)"', profile_block)
if len(registry) != 20 or len(set(registry)) != len(registry):
    raise SystemExit(f"FAIL: registry count/uniqueness invalid ({len(registry)})")
if registry != recorded:
    raise SystemExit("FAIL: build registry and recorded profile symbol order differ")
for symbol in registry:
    row = rows.get(symbol)
    if not row:
        raise SystemExit(f"FAIL: {symbol} is absent from the reviewed inventory")
    if row["modulation"] != "OOK_PULSE_PWM" and not row["modulation"].startswith("OOK_"):
        raise SystemExit(f"FAIL: non-OOK decoder registered: {symbol} ({row['modulation']})")
    if row["disabled_initializer"].strip() not in {"0 (implicit)", "0"}:
        raise SystemExit(f"FAIL: disabled initializer registered: {symbol}")
if "433_weather" in ini or "-include connect_devices.h" not in ini:
    raise SystemExit("FAIL: Connect build is not force-including its reviewed registry")
if "#define NUMOFDEVICES 20" not in devices or "#define MY_DEVICES 1" not in devices:
    raise SystemExit("FAIL: custom decoder registry/count is not active")
if 'kId[] = "wx-ook43392-rtl433-f0a8156-v1"' not in profile:
    raise SystemExit("FAIL: stable Connect OOK profile identity missing")
if 'kRevision[] = "f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8"' not in profile:
    raise SystemExit("FAIL: decoder source revision differs from reviewed pin")
print(f"PASS: {len(registry)} unique reviewed 433 MHz OOK symbols match build registry and session profile")
