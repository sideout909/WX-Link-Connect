#!/usr/bin/env python3
"""Prepare fixed-repository Connect OTA assets locally; never publish."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "sideout909/WX-Link-Connect"


def package(binary, output, version):
    if not re.fullmatch(r"[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}", version):
        raise ValueError("Connect release tags require a numeric x.y.z version")
    if any(int(part) > 65535 for part in version.split(".")):
        raise ValueError("Version components exceed the firmware policy")
    data = binary.read_bytes()
    if (not 32768 <= len(data) <= 3342336 or data[0] != 0xE9
            or int.from_bytes(data[12:14], "little") != 9
            or int.from_bytes(data[32:36], "little") != 0xABCD5432):
        raise ValueError("Expected a slot-sized ESP32-S3 application image")
    if (f"WXLink Connect {version}\0".encode() not in data
            or REPOSITORY.encode() + b"\0" not in data):
        raise ValueError("Image identity/version differs; rebuild a numeric release first")
    name = f"WXLink-Connect-v{version}"
    manifest = dict(schema=1, product="WXLink Connect", repository=REPOSITORY,
                    version=version, board="panellan_sc05_x",
                    partition_layout="default_8MB-v1", flash_bytes=8388608,
                    size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                    firmware_url=f"https://github.com/{REPOSITORY}/releases/download/v{version}/{name}.bin")
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(binary, output / (name + ".bin"))
    (output / (name + "-manifest.json")).write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--binary", type=Path, default=ROOT / "firmware/.pio/build/panellan_sc05_x/firmware.bin")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/connect-update")
    args = parser.parse_args()
    try:
        package(args.binary, args.output, args.version)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    print(f"Prepared local Connect OTA assets for v{args.version}. Nothing published.")


if __name__ == "__main__":
    main()
