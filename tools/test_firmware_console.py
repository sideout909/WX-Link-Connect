import unittest
from firmware_console import sample_command, response, screenshot, download_archive, synchronize
from pathlib import Path
import json
import tempfile


class ReplayAdapterTests(unittest.TestCase):
    def test_console_synchronizes_before_commands_and_retries_stale_error(self):
        class Port:
            def __init__(self): self.attempts = 0
            def reset_input_buffer(self): pass
            def write(self, data):
                if data != b'\n':
                    self.command = json.loads(data); self.attempts += 1
                return len(data)
            def readline(self):
                if self.attempts == 1: return b'{"event":"error","reason":"invalid_json"}\n'
                return json.dumps(dict(event='status', request_id=self.command['request_id'])).encode()
        port = Port()
        self.assertEqual(synchronize(port)['event'], 'status')
        self.assertEqual(port.attempts, 2)

    def test_archive_download_preserves_window_and_publishes_only_complete_file(self):
        class Port:
            def __init__(self): self.pending = None; self.requests = []
            def write(self, data):
                command = json.loads(data); self.requests.append(command)
                offset = command['offset']
                if offset: self.assert_snapshot = command['end'] == 5000000
                self.pending = dict(event='archive', request_id=command['request_id'], end=5000000,
                                    next_offset=32 if offset == 0 else 2016,
                                    csv='utc_start,value\n2026-10-21T00:00:00Z,0\n' if offset == 0 else '')
                return len(data)
            def readline(self): return (json.dumps(self.pending)+'\n').encode()
        port = Port()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)/'history.csv'
            self.assertEqual(download_archive(port, output), output)
            self.assertEqual(output.read_text(), 'utc_start,value\n2026-10-21T00:00:00Z,0\n')
            self.assertFalse(output.with_suffix('.csv.partial').exists())
            self.assertTrue(port.assert_snapshot)

    def test_archive_rejects_nonadvancing_pagination(self):
        class Port:
            def write(self, data): self.command = json.loads(data); return len(data)
            def readline(self): return json.dumps(dict(event='archive', request_id=self.command['request_id'], end=5000000, next_offset=0, csv='utc_start,value\n')).encode()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)/'history.csv'
            with self.assertRaisesRegex(RuntimeError, 'pagination'):
                download_archive(Port(), output)
            self.assertFalse(output.exists())

    def packet(self, **extra):
        return dict(model='Acurite-Atlas', id=679, channel='A',
                    message_type=39, time='2026-10-18T00:00:00', **extra)

    def test_sparse_zero_values_and_explicit_utc(self):
        result = sample_command(self.packet(lux=0, uvi=0))
        self.assertEqual(result['lux'], 0)
        self.assertEqual(result['uv'], 0)
        self.assertNotIn('temperature_F', result)
        self.assertEqual(result['time_ms'], 1792281600000)

    def test_native_uv_and_alias_precedence(self):
        self.assertEqual(sample_command(self.packet(uv=0))['uv'], 0)
        self.assertEqual(sample_command(self.packet(uv=7, uvi=3))['uv'], 7)
        self.assertNotIn('uv', sample_command(self.packet()))

    def test_flags_rain_and_station_filter(self):
        result = sample_command(self.packet(exception=1, rain_in=5.11))
        self.assertTrue(result['exception'])
        self.assertEqual(result['rain_tips'], 511)
        other = self.packet(); other['id'] = 680
        self.assertIsNone(sample_command(other))

    def test_screenshot_refuses_setup_access_key_view(self):
        class Port:
            def write(self, data): return len(data)
            def readline(self): return b'{"event":"error","reason":"setup_view_contains_key"}\n'
        with self.assertRaisesRegex(RuntimeError, 'setup_view_contains_key'):
            screenshot(Port(), Path('/tmp/unused-weather-screen.png'))

    def test_bad_time_is_not_replaced_with_current_time(self):
        invalid = self.packet(); invalid['time'] = 'not a timestamp'
        with self.assertRaises(ValueError): sample_command(invalid)

    def test_response_does_not_confuse_periodic_diagnostics_with_reply(self):
        class Port:
            def __init__(self):
                self.lines = iter([
                    b'{"event":"status","request_id":0,"accepted":1}\n',
                    b'{"event":"status","request_id":25,"accepted":2}\n'])
            def readline(self): return next(self.lines)
        self.assertEqual(response(Port(), 'status', request_id=25)['accepted'], 2)


if __name__ == '__main__': unittest.main()
