#!/usr/bin/env python3
"""Offline replay through pinned OOK decoders, then Connect capture and preview.

No network, serial port, SDR or hardware input is opened. Public pulse fixtures
and their independent upstream expected results are checked into the workspace.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'vendor/rtl_433_ESP'
FIXTURES = ROOT / 'firmware/tests/fixtures/public-rtl433'


def run(command):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{Path(str(command[0])).name} failed: {result.stderr}')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    sanitizers = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []
    provenance = json.loads((FIXTURES / 'provenance.json').read_text())
    symbols = __import__('re').findall(r'DECL\((\w+)\)', (ROOT / 'firmware/include/connect_devices.h').read_text())
    aliases = {'acurite_txr': 'acurite', 'lacrossetx': 'lacrosse'}
    with tempfile.TemporaryDirectory(prefix='wxconnect-public-') as tmp:
        tmp = Path(tmp)
        common = ['bitbuffer', 'bit_util', 'decoder_util', 'data', 'abuf', 'pulse_slicer', 'logger']
        sources = [VENDOR / 'src/rtl_433' / (name + '.c') for name in common]
        sources += list(dict.fromkeys(VENDOR / 'src/rtl_433/devices' / (aliases.get(s, s) + '.c') for s in symbols))
        replay, pipeline = tmp / 'replay', tmp / 'pipeline'
        run(['cc', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-O1', '-ffunction-sections', '-fdata-sections',
             '-I' + str(ROOT / 'firmware/include'), '-I' + str(VENDOR / 'include'),
             '-I' + str(VENDOR / 'src/rtl_433'), ROOT / 'firmware/tests/decoder_replay.c', *sources,
             '-Wl,--gc-sections', '-lm', *sanitizers, '-o', replay])
        registry = run([replay, '--registry']).stdout.splitlines()
        capacity = int(__import__('re').search(r'kProtocolFieldCapacity = (\d+)',
            (ROOT / 'firmware/include/capture_devices.h').read_text())[1])
        assert len(registry) == len(symbols) == 20
        assert all(len(row.split('\t', 2)[2].encode('utf-8')) < capacity for row in registry)
        print(f'PASS: all 20 compiled decoder names fit the bounded protocol identity ({capacity - 1} bytes)')
        run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'firmware/include'),
             '-I' + str(ROOT / 'vendor/ArduinoJson/src'), ROOT / 'firmware/tests/public_decoder_pipeline_test.cpp',
             *sanitizers, '-o', pipeline])
        total = 0
        for fixture in provenance['fixtures']:
            pulse = FIXTURES / (fixture['name'] + '.ook')
            assert hashlib.sha256(pulse.read_bytes()).hexdigest() == fixture['pulse_sha256']
            result = run([replay, pulse])
            actual = [json.loads(line) for line in result.stdout.splitlines()]
            expected = json.loads((FIXTURES / (fixture['name'] + '.expected.json')).read_text())
            assert len(actual) == len(expected) == fixture['count'], fixture['name']
            for got, want in zip(actual, expected):
                assert got['model'] == want['model'] == fixture['model']
                for field, value in want.items():
                    if field in ('time', 'protocol'):
                        continue
                    assert field in got, (fixture['name'], field, 'missing')
                    if isinstance(value, float):
                        assert math.isclose(got[field], value, rel_tol=1e-4, abs_tol=0.025), (fixture['name'], field, got[field], value)
                    else:
                        assert got[field] == value, (fixture['name'], field, got[field], value)
            output = tmp / 'native.jsonl'
            output.write_text(result.stdout)
            run([pipeline, output])
            total += len(actual)
            print(f'PASS: {fixture["model"]}: {len(actual)} public packets; repeated capture, exact JSON, preview isolation')
        print(f'PASS: {len(provenance["fixtures"])} public waveform fixtures, {total} pinned-decoder packets; no hardware compatibility claim')


if __name__ == '__main__':
    main()
