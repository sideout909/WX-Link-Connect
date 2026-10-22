import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parent))
import connect_capture


def _diag(diagnostics, key):
    return diagnostics.get(key) if isinstance(diagnostics, dict) else None


class FakePort:
    def __init__(self, lines=()):
        self.lines = list(lines)
        self.writes = []

    def write(self, data):
        self.writes.append(data)

    def read_until(self, separator, size):
        if not self.lines:
            return b""
        return self.lines.pop(0)


class CaptureCollectorTests(unittest.TestCase):
    SESSION = "0123456789abcdef0123456789abcdef"
    PROFILE = "wx-ook43392-rtl433-f0a8156-v1"

    def envelope(self, seq=0, event="packet", identity="1", **extra):
        item = {"schema": 1, "event": event, "session_id": self.SESSION,
                "seq": seq, "uptime_ms": seq * 100, "utc_ms": None,
                "time_valid": False, "source": "live", "profile_id": self.PROFILE}
        item.update(extra)
        if event == "packet":
            item.setdefault("native_json", json.dumps({"model": "AcuRite", "id": identity,
                                                          "channel": 1, "temperature_C": 0}))
        if event == "pulse":
            item.update(pulse_us=[100], gap_us=[50], original_count=1,
                        captured_count=1, truncated=False, duration_us=150,
                        packet_seq=None)
        return item

    def base_records(self):
        session = self.envelope(0, "session", decoder_library="rtl_433_ESP",
                                decoder_revision="f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8",
                                profile_revision=1, requested_center_frequency_mhz=433.92,
                                requested_modulation="OOK", requested_rx_bandwidth_khz=812,
                                requested_rssi_margin_db=9,
                                radio_initialization_state="init_accepted_readback_unverified",
                                radio_readback_verified=False,
                                firmware="WXLink Connect 0.1.0", board="panellan_sc05_x",
                                decoder_symbols=["acurite_txr"])
        end = self.envelope(3, "end", capture_diagnostics={
            "decoder_truncation_drops": 0, "queue_full_drops": 0,
            "oversize_drops": 0, "invalid_json_drops": 0, "storage_drops": 0,
            "transport_drops": 0, "receiver_drops": 0,
            "raw_callback_drops": 0, "raw_callback_skips": 0})
        return [session, self.envelope(1), self.envelope(2, identity="2"), end]

    def write_bundle(self, directory, records=None, pulses=None):
        directory.mkdir()
        records = self.base_records() if records is None else records
        decoded = "".join(json.dumps(record, separators=(",", ":")) + "\n" for record in records).encode()
        (directory / "decoded.jsonl").write_bytes(decoded)
        files = {"decoded.jsonl": {"bytes": len(decoded), "sha256": hashlib.sha256(decoded).hexdigest()}}
        pulse_count = 0
        pulse_truncated = 0
        if pulses is not None:
            pulse_bytes = "".join(json.dumps(record, separators=(",", ":")) + "\n" for record in pulses).encode()
            (directory / "pulses.jsonl").write_bytes(pulse_bytes)
            files["pulses.jsonl"] = {"bytes": len(pulse_bytes), "sha256": hashlib.sha256(pulse_bytes).hexdigest()}
            pulse_count = len(pulses)
            pulse_truncated = sum(record.get("truncated") is True for record in pulses)
        session_diagnostics = next((record.get("capture_diagnostics") for record in records
                                    if record.get("event") == "end"), None)
        diagnostics = {"capture_session_diagnostics": session_diagnostics,
                       "transport": {"malformed_lines": 0, "oversized_lines": 0,
                                     "unmatched_events": 0}}
        diagnostics_bytes = (json.dumps(diagnostics, sort_keys=True, indent=2) + "\n").encode()
        (directory / "diagnostics.json").write_bytes(diagnostics_bytes)
        files["diagnostics.json"] = {"bytes": len(diagnostics_bytes), "sha256": hashlib.sha256(diagnostics_bytes).hexdigest()}
        manifest = {"schema": 1, "session_id": self.SESSION, "source": "live",
                    "profile_id": self.PROFILE, "firmware": records[0].get("firmware"),
                    "board": records[0].get("board"), "files": files,
                    "field_notes": connect_capture._clean_field_notes({}),
                    "loss_totals": {"decoder_truncation_drops": _diag(session_diagnostics, "decoder_truncation_drops"),
                                    "queue_full_drops": _diag(session_diagnostics, "queue_full_drops"),
                                    "oversize_drops": _diag(session_diagnostics, "oversize_drops"),
                                    "invalid_json_drops": _diag(session_diagnostics, "invalid_json_drops"),
                                    "storage_drops": _diag(session_diagnostics, "storage_drops"),
                                    "transport_drops": _diag(session_diagnostics, "transport_drops"),
                                    "receiver_drops": _diag(session_diagnostics, "receiver_drops"),
                                    "raw_callback_drops": _diag(session_diagnostics, "raw_callback_drops"),
                                    "raw_callback_skips": _diag(session_diagnostics, "raw_callback_skips"),
                                    "raw_pulse_truncated_records": pulse_truncated,
                                    "host_malformed_lines": 0, "host_oversized_lines": 0,
                                    "host_unmatched_events": 0},
                    "record_counts": {"decoded": len(records), "pulses": pulse_count},
                    "interrupted": not any(row["event"] == "end" for row in records),
                    "sequence_loss": False, "privacy_review_required": True,
                    "submission": "manual only; inspect neighbor identifiers and timing before sharing",
                    "reviewed_derivative": None}
        (directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        return manifest

    def test_command_skips_wrong_reply_id_and_periodic_status_noise(self):
        port = FakePort([
            (json.dumps({"event": "capture_status", "request_id": 90}) + "\n").encode(),
            (json.dumps({"event": "status", "request_id": 1, "periodic": True}) + "\n").encode(),
            (json.dumps({"event": "capture_control", "request_id": 1, "ok": True}) + "\n").encode(),
        ])
        collector = connect_capture.Collector(port)
        self.assertTrue(collector.command("capture_start")["ok"])
        self.assertEqual(collector.noise_events, 2)

    def test_export_uses_the_selected_session_end_diagnostics_and_build_identity(self):
        class StoredCollector:
            malformed_lines = oversized_lines = noise_events = 0
            def __init__(self, rows):
                self.rows = rows
                self.pending_record = None
                self.indices = {}
            def command(self, op, timeout=5, **fields):
                assert op == "capture_list"
                return {"sessions": [{"session_id": sid, "decoded_records": len(records),
                                      "pulse_records": 0, "interrupted": False}
                                     for sid, records in self.rows.items()]}
            def read_stored_record(self, session_id, stream, offset):
                rows = self.rows[session_id]
                index = self.indices.get(session_id, 0)
                item = dict(rows[index]); item.pop("_offset")
                self.pending_record = (json.dumps(item, separators=(",", ":")) + "\n").encode()
                next_offset = offset + len(self.pending_record)
                self.indices[session_id] = index + 1
                return item, {"next_offset": next_offset, "done": index + 1 == len(rows)}

        a = self.base_records()
        for record in a: record["session_id"] = "a" * 32
        a[3]["capture_diagnostics"]["decoder_truncation_drops"] = 7
        a[3]["capture_diagnostics"]["receiver_drops"] = 2
        b = self.base_records()
        for record in b: record["session_id"] = "b" * 32
        b[3]["capture_diagnostics"]["decoder_truncation_drops"] = 23
        b[0]["firmware"] = "WXLink Connect newer build"
        rows = {sid: [{**record, "_offset": idx} for idx, record in enumerate(records)]
                for sid, records in (("a" * 32, a), ("b" * 32, b))}
        collector = StoredCollector(rows)
        with tempfile.TemporaryDirectory() as temporary:
            bundle = connect_capture.export_bundle(collector, "a" * 32, Path(temporary),
                                                   {"version": "current B", "board": "other"})
            manifest = json.loads((bundle / "manifest.json").read_text())
            diagnostics = json.loads((bundle / "diagnostics.json").read_text())
            self.assertEqual(manifest["loss_totals"]["decoder_truncation_drops"], 7)
            self.assertEqual(manifest["loss_totals"]["receiver_drops"], 2)
            self.assertEqual(manifest["firmware"], "WXLink Connect 0.1.0")
            self.assertEqual(manifest["board"], "panellan_sc05_x")
            self.assertEqual(diagnostics["capture_session_diagnostics"]["decoder_truncation_drops"], 7)
            self.assertNotIn("device_capture_status", diagnostics)

    def test_pre_repair_session_exports_unavailable_historical_loss_as_null(self):
        class LegacyCollector:
            malformed_lines = oversized_lines = noise_events = 0
            def __init__(self, rows): self.rows, self.index, self.pending_record = rows, 0, None
            def command(self, op, timeout=5, **fields):
                return {"sessions": [{"session_id": CaptureCollectorTests.SESSION,
                                      "decoded_records": len(self.rows), "pulse_records": 0,
                                      "interrupted": False}]}
            def read_stored_record(self, session_id, stream, offset):
                item = self.rows[self.index]; self.index += 1
                self.pending_record = (json.dumps(item, separators=(",", ":")) + "\n").encode()
                return item, {"next_offset": offset + len(self.pending_record),
                              "done": self.index == len(self.rows)}

        records = self.base_records()
        for item in records:
            item.pop("firmware", None); item.pop("board", None)
        records[-1].pop("capture_diagnostics", None)
        collector = LegacyCollector(records)
        with tempfile.TemporaryDirectory() as temporary:
            bundle = connect_capture.export_bundle(collector, self.SESSION, Path(temporary),
                                                   {"version": "current firmware", "board": "current board"})
            manifest = json.loads((bundle / "manifest.json").read_text())
            self.assertIsNone(manifest["firmware"])
            self.assertIsNone(manifest["board"])
            self.assertIsNone(manifest["loss_totals"]["decoder_truncation_drops"])
            self.assertIsNone(manifest["loss_totals"]["receiver_drops"])

    def test_malformed_and_truncated_lines_are_counted(self):
        port = FakePort([b"{broken}\n", b"x" * (connect_capture.MAX_LINE_BYTES + 1),
                         (json.dumps({"event": "status", "request_id": 1}) + "\n").encode()])
        collector = connect_capture.Collector(port)
        self.assertEqual(collector.command("status")["event"], "status")
        self.assertEqual(collector.malformed_lines, 1)
        self.assertEqual(collector.oversized_lines, 1)

    def test_envelope_preserves_duplicates_but_rejects_wrong_identity(self):
        self.assertTrue(connect_capture.valid_envelope(self.envelope(seq=3), self.SESSION)[0])
        self.assertTrue(connect_capture.valid_envelope(self.envelope(seq=4), self.SESSION)[0])
        wrong = self.envelope()
        wrong["session_id"] = "f" * 32
        self.assertFalse(connect_capture.valid_envelope(wrong, self.SESSION)[0])
        oversized = self.envelope(native_json=json.dumps({"payload": "x" * 2048}))
        self.assertFalse(connect_capture.valid_envelope(oversized, self.SESSION)[0])
        wrong_bool = self.envelope()
        wrong_bool["seq"] = True
        self.assertFalse(connect_capture.valid_envelope(wrong_bool, self.SESSION)[0])

    def test_pulse_bounds_and_lengths_and_truncation_are_checked(self):
        pulse = self.envelope(1, "pulse")
        pulse.update(original_count=3, captured_count=1, truncated=True)
        self.assertTrue(connect_capture.valid_envelope(pulse, self.SESSION)[0])
        pulse["truncated"] = False
        self.assertFalse(connect_capture.valid_envelope(pulse, self.SESSION)[0])
        pulse["truncated"] = True
        pulse["gap_us"] = []
        self.assertFalse(connect_capture.valid_envelope(pulse, self.SESSION)[0])

    def test_stored_jsonl_line_is_kept_byte_for_byte(self):
        record = self.envelope(native_json='{"unknown":"\\\\u0061"}')
        raw = (json.dumps(record, separators=(",", ":"), ensure_ascii=False) + "\n").encode()
        ack = {"event": "capture_read", "request_id": 1, "ok": True,
               "stream": "decoded", "offset": 0, "next_offset": 128, "done": True}
        collector = connect_capture.Collector(FakePort([raw, (json.dumps(ack) + "\n").encode()]))
        actual, reply = collector.read_stored_record(self.SESSION, "decoded", 0)
        self.assertEqual(actual["native_json"], '{"unknown":"\\\\u0061"}')
        self.assertEqual(collector.pending_record, raw)
        self.assertTrue(reply["done"])


    def test_live_session_requires_decoder_and_rf_provenance(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "bundle"
            records = self.base_records()
            for key in ("decoder_library", "decoder_revision", "profile_revision",
                        "requested_center_frequency_mhz", "requested_modulation",
                        "requested_rx_bandwidth_khz", "requested_rssi_margin_db",
                        "radio_initialization_state", "radio_readback_verified", "decoder_symbols"):
                records[0].pop(key)
            manifest = self.write_bundle(directory, records=records)
            with self.assertRaises(connect_capture.CaptureError):
                connect_capture._validate_bundle(directory, self.SESSION, manifest)

    def test_bundle_validation_checks_hash_schema_sequence_counts_and_session(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "bundle"
            manifest = self.write_bundle(directory)
            connect_capture._validate_bundle(directory, self.SESSION, manifest)
            manifest["record_counts"]["decoded"] -= 1
            with self.assertRaises(connect_capture.CaptureError):
                connect_capture._validate_bundle(directory, self.SESSION, manifest)
            manifest["record_counts"]["decoded"] += 1
            data = (directory / "decoded.jsonl").read_bytes() + b" "
            (directory / "decoded.jsonl").write_bytes(data)
            with self.assertRaises(connect_capture.CaptureError):
                connect_capture._validate_bundle(directory, self.SESSION, manifest)



    def test_combined_sequence_accounts_for_interleaved_pulse_stream(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "bundle"
            records = self.base_records()
            records[1]["seq"] = 2
            records[2]["seq"] = 3
            records[3]["seq"] = 4
            pulse = self.envelope(1, "pulse")
            manifest = self.write_bundle(directory, records=records, pulses=[pulse])
            connect_capture._validate_bundle(directory, self.SESSION, manifest)
            manifest["sequence_loss"] = True
            with self.assertRaises(connect_capture.CaptureError):
                connect_capture._validate_bundle(directory, self.SESSION, manifest)

    def test_validator_rejects_private_manifest_keys(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "bundle"
            manifest = self.write_bundle(directory)
            manifest["wifi_password"] = "never export"
            with self.assertRaises(connect_capture.CaptureError):
                connect_capture._validate_bundle(directory, self.SESSION, manifest)

    def test_private_diagnostics_and_partial_lines_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "bundle"
            manifest = self.write_bundle(directory)
            diagnostics = {"admin_password": "must never be exported"}
            data = json.dumps(diagnostics).encode()
            (directory / "diagnostics.json").write_bytes(data)
            manifest["files"]["diagnostics.json"] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            (directory / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaises(connect_capture.CaptureError):
                connect_capture._validate_bundle(directory, self.SESSION, manifest)

    def test_selected_review_derivative_preserves_native_strings_and_source_bundle(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            manifest = self.write_bundle(source, pulses=[self.envelope(4, "pulse")])
            original = (source / "decoded.jsonl").read_bytes()
            destination = Path(temporary) / "selected"
            result = connect_capture._review_derivative(source, destination,
                ("", "AcuRite", "1", "1"), "review_for_public_sharing")
            derivative = json.loads((result / "manifest.json").read_text())
            rows = [json.loads(line) for line in (result / "decoded.jsonl").read_text().splitlines()]
            packets = [row for row in rows if row["event"] == "packet"]
            self.assertEqual(len(packets), 1)
            self.assertEqual(packets[0]["native_json"], json.loads(original.splitlines()[1])["native_json"])
            self.assertFalse((result / "pulses.jsonl").exists())
            self.assertEqual((source / "decoded.jsonl").read_bytes(), original)
            self.assertEqual(derivative["reviewed_derivative"]["excluded_packet_records"], 1)
            self.assertTrue(derivative["reviewed_derivative"]["native_payloads_modified"] is False)
            connect_capture._validate_bundle(result, self.SESSION, derivative)


    def test_explicit_native_id_redaction_creates_hashed_derivative_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            self.write_bundle(source)
            original = (source / "decoded.jsonl").read_bytes()
            destination = Path(temporary) / "redacted"
            result = connect_capture._review_derivative(source, destination,
                "all", "review_for_public_sharing", redact=True)
            manifest = json.loads((result / "manifest.json").read_text())
            packets = [json.loads(line) for line in (result / "decoded.jsonl").read_text().splitlines()
                       if json.loads(line)["event"] == "packet"]
            self.assertTrue(all(json.loads(record["native_json"])["id"] == "[REDACTED]" for record in packets))
            self.assertEqual((source / "decoded.jsonl").read_bytes(), original)
            self.assertTrue(manifest["reviewed_derivative"]["native_payloads_modified"])
            self.assertIn("native_key_redaction", manifest["reviewed_derivative"]["transformation"])
            connect_capture._validate_bundle(result, self.SESSION, manifest)

    def test_manual_review_requires_typed_confirmation_and_all_device_choice(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            self.write_bundle(source)
            answers = iter(["a", "public-review", "n", f"REVIEW {self.SESSION}"])
            result = connect_capture.review_bundle(source, input_fn=lambda _: next(answers), output_fn=lambda _: None)
            manifest = json.loads((result / "manifest.json").read_text())
            self.assertEqual(manifest["reviewed_derivative"]["transformation"], "all_device_review")
            self.assertEqual(manifest["record_counts"]["decoded"], 4)
            self.assertEqual(manifest["reviewed_derivative"]["manual_public_sharing_choice"], "review_for_public_sharing")

    def test_field_notes_are_bounded_and_do_not_accept_non_text(self):
        with self.assertRaises(connect_capture.CaptureError):
            connect_capture._clean_field_notes({"station_make": 1})
        with self.assertRaises(connect_capture.CaptureError):
            connect_capture._clean_field_notes({"observation_notes": "x" * 501})
        with self.assertRaises(connect_capture.CaptureError):
            connect_capture._clean_field_notes({"region": "latitude 42.123456, longitude -83.123456"})


if __name__ == "__main__":
    unittest.main()
