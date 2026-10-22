#!/usr/bin/env python3
"""Repack existing original PNG assets as firmware flash arrays (no image edits)."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
names = ['sunny', 'partly-cloudy', 'cloudy', 'rain', 'thunderstorm', 'snow', 'fog', 'windy', 'unknown']
with (root/'firmware/include/weather_icons.h').open('w') as out:
    out.write('#pragma once\n#include <Arduino.h>\n// Packed from the project\'s original assets/weather/*.png.\nnamespace weather_icons {\n')
    for name in names:
        data = (root/'assets/weather'/f'{name}.png').read_bytes()
        out.write('static const uint8_t '+name.replace('-', '_')+'[] PROGMEM = {\n')
        for offset in range(0, len(data), 24):
            out.write('  '+','.join(f'0x{value:02x}' for value in data[offset:offset+24])+',\n')
        out.write('};\n')
    out.write('}\n')
