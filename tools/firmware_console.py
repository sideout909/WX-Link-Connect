#!/usr/bin/env python3
"""USB development console. Replay is explicitly recorded data, never live RF."""
import argparse
from datetime import datetime, timezone
import gzip
import json
from pathlib import Path
import struct
import time
import zlib

DEFAULT_PORT = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_68:B6:B3:34:82:48-if00'
FIELD_MAP = {'temperature_F': 'temperature_F', 'humidity': 'humidity',
             'wind_avg_mi_h': 'wind_mph', 'wind_dir_deg': 'direction_deg',
             'lux': 'lux', 'uvi': 'uv', 'uv': 'uv', 'strike_count': 'strikes'}


def sample_command(packet):
    if (packet.get('model'), packet.get('id'), packet.get('channel')) != ('Acurite-Atlas', 679, 'A'):
        return None
    stamp = datetime.fromisoformat(packet['time'].replace('Z', '+00:00'))
    if stamp.tzinfo is None:
        stamp = stamp.replace(tzinfo=timezone.utc)
    result = {'op': 'sample', 'time_ms': round(stamp.timestamp()*1000),
              'station': 679, 'channel': 'A', 'type': packet['message_type'],
              'exception': bool(packet.get('exception', 0))}
    for source, dest in FIELD_MAP.items():
        if source in packet and packet[source] is not None:
            result[dest] = packet[source]
    if 'rain_in' in packet and packet['rain_in'] is not None:
        result['rain_tips'] = round(packet['rain_in']*100)
    return result


def send(port, command):
    command = dict(command, request_id=time.monotonic_ns() & 0xffffffff)
    port.write((json.dumps(command, separators=(',', ':'), allow_nan=False)+'\n').encode())
    return command['request_id']


def response(port, event, timeout=5, request_id=None):
    deadline = time.monotonic()+timeout
    while time.monotonic() < deadline:
        line = port.readline()
        try:
            item = json.loads(line)
        except (ValueError, UnicodeDecodeError):
            continue
        if item.get('event') == 'fatal':
            raise RuntimeError(item)
        if item.get('event') == event and (request_id is None or item.get('request_id') == request_id):
            return item
        if item.get('event') == 'error':
            raise RuntimeError(item)
    raise TimeoutError(f'No {event} response from firmware')


def download_archive(port, output):
    """Page the fixed archive without pausing RF or loading it all on the ESP32."""
    output.parent.mkdir(parents=True, exist_ok=True)
    partial = output.with_suffix(output.suffix+'.partial')
    offset = 0
    end = None
    with partial.open('w', encoding='utf-8', newline='') as stream:
        while offset < 2016:
            command = dict(op='archive', offset=offset)
            if end is not None:
                command['end'] = end
            rid = send(port, command)
            reply = response(port, 'archive', timeout=10, request_id=rid)
            if end is None:
                end = reply['end']
            if reply['end'] != end:
                raise RuntimeError('Archive window changed during download')
            next_offset = reply['next_offset']
            if not isinstance(next_offset, int) or not offset < next_offset <= 2016:
                raise RuntimeError('Invalid archive pagination')
            csv = reply['csv']
            if not isinstance(csv, str) or (offset == 0 and not csv.startswith('utc_start,')):
                raise RuntimeError('Invalid archive CSV response')
            stream.write(csv)
            offset = next_offset
    partial.replace(output)
    return output


def synchronize(port):
    # Native USB can still be finishing its control-line transition when open
    # returns. Establish an acknowledged connection before any stateful command.
    time.sleep(.1)
    port.reset_input_buffer()
    for attempt in range(3):
        port.write(b'\n')
        rid = send(port, {'op': 'status'})
        try:
            return response(port, 'status', timeout=5, request_id=rid)
        except (TimeoutError, RuntimeError) as error:
            if error.args and isinstance(error.args[0], dict) and error.args[0].get('event') == 'fatal':
                raise
            if attempt == 2:
                raise


def screenshot(port, output):
    send(port, {'op': 'screen'})
    deadline = time.monotonic()+8
    while time.monotonic() < deadline:
        line = port.readline().strip()
        if line == b'SCREEN RGB888 320 240 230400':
            break
        try:
            message = json.loads(line)
        except (ValueError, UnicodeDecodeError):
            continue
        if message.get('event') in ('error', 'fatal'):
            raise RuntimeError(message)
    else:
        raise TimeoutError('No screenshot header')
    pixels = port.read(320*240*3)
    if len(pixels) != 320*240*3:
        raise RuntimeError(f'Incomplete screenshot: {len(pixels)} bytes')
    def chunk(kind, data):
        return struct.pack('!I', len(data))+kind+data+struct.pack('!I', zlib.crc32(kind+data))
    scanlines = b''.join(b'\0'+pixels[y*960:(y+1)*960] for y in range(240))
    png = (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('!2I5B', 320, 240, 8, 2, 0, 0, 0))+
           chunk(b'IDAT', zlib.compress(scanlines))+chunk(b'IEND', b''))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(png)
    print(output)


def main():
    import serial

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default=DEFAULT_PORT)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('status')
    sub.add_parser('diagnostics')
    sub.add_parser('reset')
    sub.add_parser('checkpoint-save')
    archive = sub.add_parser('archive'); archive.add_argument('output', type=Path)
    page = sub.add_parser('page'); page.add_argument('page', choices=['home', 'trends', 'forecast'])
    detail = sub.add_parser('detail'); detail.add_argument('detail', choices=['outdoor', 'wind', 'humidity', 'rain', 'pressure', 'link'])
    screen = sub.add_parser('screen'); screen.add_argument('output', type=Path)
    replay = sub.add_parser('replay')
    replay.add_argument('input', type=Path)
    replay.add_argument('--start', help='ISO-8601 UTC lower bound, inclusive')
    replay.add_argument('--end', help='ISO-8601 UTC upper bound, exclusive')
    replay.add_argument('--limit', type=int, default=1000, help='Maximum matching packets to send')
    args = parser.parse_args()
    # The S3 native USB bridge resets with both lines deasserted. Keeping
    # DTR/RTS asserted together preserves the running app on this board.
    port = serial.Serial(port=None, baudrate=115200, timeout=3, write_timeout=5, exclusive=True)
    port.dtr = True; port.rts = True; port.port = args.port
    with port:
        synchronize(port)
        if args.command == 'archive':
            print(download_archive(port, args.output))
        elif args.command == 'screen':
            screenshot(port, args.output)
        elif args.command == 'replay':
            rid = send(port, {'op': 'reset'}); response(port, 'reset', request_id=rid)
            sent = 0; outcomes = {}
            opener = gzip.open if args.input.suffix == '.gz' else open
            def bound(text):
                stamp = datetime.fromisoformat(text.replace('Z', '+00:00'))
                if stamp.tzinfo is None: stamp = stamp.replace(tzinfo=timezone.utc)
                return round(stamp.timestamp()*1000)
            lower = bound(args.start) if args.start else None
            upper = bound(args.end) if args.end else None
            with opener(args.input, 'rt') as stream:
                for line in stream:
                    if sent >= args.limit: break
                    item = sample_command(json.loads(line))
                    if item is None: continue
                    if lower is not None and item['time_ms'] < lower: continue
                    if upper is not None and item['time_ms'] >= upper: continue
                    item['request_id'] = time.monotonic_ns() & 0xffffffff
                    encoded = (json.dumps(item, separators=(',', ':'), allow_nan=False)+'\n').encode()
                    for attempt in range(3):
                        port.write(encoded)
                        try:
                            reply = response(port, 'sample', request_id=item['request_id'])
                            break
                        except TimeoutError:
                            if attempt == 2: raise
                    result = str(reply['result']); outcomes[result] = outcomes.get(result, 0)+1
                    sent += 1
            rid = send(port, {'op': 'status'})
            print(json.dumps({'sent': sent, 'results': outcomes, 'status': response(port, 'status', request_id=rid)}, indent=2))
        else:
            op = {'op': args.command.replace('-', '_')}
            if args.command == 'page': op['page'] = ['home', 'trends', 'forecast'].index(args.page)
            if args.command == 'detail': op['detail'] = ['outdoor', 'wind', 'humidity', 'rain', 'pressure', 'link'].index(args.detail)
            rid = send(port, op)
            # Flash write/replace/CRC diagnostics can exceed the ordinary
            # five-second reply budget while network work is also running.
            timeout = 45 if args.command == 'diagnostics' else 15 if args.command == 'checkpoint-save' else 5
            print(json.dumps(response(port, op['op'], timeout=timeout, request_id=rid), indent=2))


if __name__ == '__main__':
    main()
