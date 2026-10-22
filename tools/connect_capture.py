#!/usr/bin/env python3
"""Collect and validate WXLink Connect sessions over its native USB serial link."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time

MAX_LINE_BYTES = 65536
SESSION_RE = re.compile(r"^[0-9a-f]{32}$")
STREAMS = ("decoded", "pulses")
REPLY_EVENTS = {"status": "status", "capture_start": "capture_control",
                "capture_stop": "capture_control", "capture_status": "capture_status",
                "capture_keepalive": "capture_heartbeat", "capture_list": "capture_list",
                "capture_delete": "capture_delete", "raw_window_start": "raw_window"}


class CaptureError(RuntimeError):
    pass


def valid_envelope(item, session_id):
    if not isinstance(item, dict):
        return False, "record is not an object"
    required = ("schema", "event", "session_id", "seq", "uptime_ms", "utc_ms",
                "time_valid", "source", "profile_id")
    if any(key not in item for key in required):
        return False, "missing envelope field"
    if (not isinstance(item["schema"], int) or isinstance(item["schema"], bool) or item["schema"] != 1 or
            item["session_id"] != session_id or not SESSION_RE.fullmatch(item["session_id"])):
        return False, "schema or session identity mismatch"
    if item["event"] not in ("session", "packet", "pulse", "status", "gap", "end"):
        return False, "unknown event"
    if not isinstance(item["seq"], int) or isinstance(item["seq"], bool) or not 0 <= item["seq"] <= 0xffffffffffffffff:
        return False, "invalid sequence"
    if not isinstance(item["uptime_ms"], int) or isinstance(item["uptime_ms"], bool) or not 0 <= item["uptime_ms"] <= 0xffffffffffffffff:
        return False, "invalid uptime"
    if not isinstance(item["time_valid"], bool):
        return False, "invalid time_valid"
    if item["time_valid"]:
        if not isinstance(item["utc_ms"], int) or isinstance(item["utc_ms"], bool) or not 0 <= item["utc_ms"] <= 0xffffffffffffffff:
            return False, "valid UTC timestamp is missing"
    elif item["utc_ms"] is not None:
        return False, "invalid UTC timestamp must be null"
    if item["source"] not in ("live", "synthetic", "replay"):
        return False, "invalid source"
    if not isinstance(item["profile_id"], str) or not item["profile_id"] or len(item["profile_id"].encode("utf-8")) >= 48:
        return False, "invalid profile"
    if item["event"] == "packet":
        if not isinstance(item.get("native_json"), str):
            return False, "packet native_json is missing"
        try:
            native = json.loads(item["native_json"])
        except (TypeError, ValueError):
            return False, "packet native_json is malformed"
        if not isinstance(native, dict):
            return False, "packet native_json is not an object"
        if len(item["native_json"].encode("utf-8")) > 2047:
            return False, "packet native_json exceeds firmware capture bounds"
    if item["event"] == "pulse":
        pulses, gaps = item.get("pulse_us"), item.get("gap_us")
        if not isinstance(pulses, list) or not isinstance(gaps, list) or len(pulses) != len(gaps):
            return False, "invalid pulse arrays"
        if len(pulses) > 512 or any(not isinstance(value, int) or isinstance(value, bool) or value < 0 or value > 0xffffffff for value in pulses + gaps):
            return False, "pulse array exceeds bounds or has invalid values"
        for name in ("original_count", "captured_count", "duration_us"):
            if not isinstance(item.get(name), int) or isinstance(item[name], bool) or item[name] < 0:
                return False, "invalid pulse metadata"
        if item["captured_count"] != len(pulses) or item["original_count"] < item["captured_count"]:
            return False, "pulse count metadata mismatch"
        if not isinstance(item.get("truncated"), bool) or item.get("packet_seq") is not None:
            return False, "invalid pulse relationship metadata"
        if item["truncated"] != (item["original_count"] > item["captured_count"]):
            return False, "pulse truncation marker disagrees with counts"
    return True, ""


class Collector:
    def __init__(self, port, now=time.monotonic):
        self.port = port
        self.now = now
        self.request_id = 0
        self.malformed_lines = 0
        self.oversized_lines = 0
        self.noise_events = 0
        self.pending_record = None
        self.last_raw_line = None

    def _read_item(self, timeout):
        deadline = self.now() + timeout
        while self.now() < deadline:
            raw = self.port.read_until(b"\n", MAX_LINE_BYTES + 1)
            if not raw:
                continue
            if len(raw) > MAX_LINE_BYTES or not raw.endswith(b"\n"):
                self.oversized_lines += 1
                continue
            try:
                item = json.loads(raw)
            except (ValueError, UnicodeDecodeError):
                self.malformed_lines += 1
                continue
            if not isinstance(item, dict):
                self.malformed_lines += 1
                continue
            self.last_raw_line = raw
            return item
        raise TimeoutError("Timed out waiting for firmware response")

    def command(self, op, timeout=5, **fields):
        self.request_id = (self.request_id + 1) & 0xffffffff
        rid = self.request_id
        payload = dict(fields, op=op, request_id=rid)
        encoded = (json.dumps(payload, separators=(",", ":"), allow_nan=False) + "\n").encode()
        self.port.write(encoded)
        deadline = self.now() + timeout
        while self.now() < deadline:
            item = self._read_item(max(0.05, deadline - self.now()))
            if item.get("request_id") != rid or item.get("event") != REPLY_EVENTS.get(op):
                self.noise_events += 1
                continue
            if item.get("event") == "error":
                raise CaptureError(str(item))
            if item.get("event") == "capture_control" and not item.get("ok", False):
                raise CaptureError(str(item))
            return item
        raise TimeoutError(f"Timed out waiting for {op} acknowledgement")

    def sync(self):
        status = self.command("status")
        if status.get("event") != "status":
            raise CaptureError("Firmware did not return status")
        capture = self.command("capture_status")
        if capture.get("event") != "capture_status":
            raise CaptureError("Firmware did not return capture status")
        return status, capture

    def keepalive(self, expected_session):
        reply = self.command("capture_keepalive")
        if reply.get("event") != "capture_heartbeat" or not reply.get("active") or reply.get("session_id") != expected_session:
            raise CaptureError("Capture lease ended or session changed")

    def read_stored_record(self, session_id, stream, offset):
        self.request_id = (self.request_id + 1) & 0xffffffff
        rid = self.request_id
        payload = dict(op="capture_read", request_id=rid, session_id=session_id,
                       stream=stream, offset=offset)
        self.port.write((json.dumps(payload, separators=(",", ":")) + "\n").encode())
        deadline = self.now() + 10
        record = None
        while self.now() < deadline:
            item = self._read_item(max(0.05, deadline - self.now()))
            if item.get("event") == "capture_read" and item.get("request_id") == rid:
                if not item.get("ok"):
                    return None, item
                return record, item
            if item.get("event") in ("session", "packet", "pulse", "status", "gap", "end"):
                if record is not None:
                    raise CaptureError("Firmware returned multiple records for one read")
                record = item
                self.pending_record = self.last_raw_line
            else:
                self.noise_events += 1
        raise TimeoutError("Timed out reading stored record")


def _write_stream(collector, partial, session_id, stream, expected_records):
    path = partial / ("decoded.jsonl" if stream == "decoded" else "pulses.jsonl")
    if expected_records == 0:
        return None
    digest = hashlib.sha256()
    counts = {}
    gaps = []
    previous_seq = None
    source = None
    offset = 0
    total = 0
    with path.open("xb") as output:
        for _ in range(expected_records):
            record, ack = collector.read_stored_record(session_id, stream, offset)
            if record is None:
                raise CaptureError(f"Stored {stream} stream ended at offset {offset}")
            ok, reason = valid_envelope(record, session_id)
            if not ok:
                raise CaptureError(f"Invalid stored {stream} record: {reason}")
            if source is None:
                source = record["source"]
            elif source != record["source"]:
                raise CaptureError("Capture source changed within a session")
            if stream == "pulses" and record["event"] != "pulse":
                raise CaptureError("Non-pulse record found in pulse stream")
            if stream == "decoded" and record["event"] == "pulse":
                raise CaptureError("Pulse record found in decoded stream")
            sequence = record["seq"]
            if previous_seq is not None:
                if sequence <= previous_seq:
                    raise CaptureError("Stored sequence is not strictly increasing")
                if sequence > previous_seq + 1:
                    gaps.append({"after_seq": previous_seq, "before_seq": sequence,
                                 "missing": sequence - previous_seq - 1})
            previous_seq = sequence
            raw = collector.pending_record
            collector.pending_record = None
            if not isinstance(raw, bytes) or not raw.endswith(b"\n"):
                raise CaptureError("Stored record was not preserved as a complete JSONL line")
            raw.decode("utf-8")
            output.write(raw)
            digest.update(raw)
            total += len(raw)
            counts[record["event"]] = counts.get(record["event"], 0) + 1
            offset = ack.get("next_offset")
            if not isinstance(offset, int) or offset <= 0:
                raise CaptureError("Invalid stored-record offset")
            if bool(ack.get("done")) != (_ + 1 == expected_records):
                raise CaptureError("Stored stream record count changed during download")
    return {"file": path.name, "bytes": total, "sha256": digest.hexdigest(),
            "records": sum(counts.values()), "events": counts, "sequence_gaps": gaps,
            "ended": counts.get("end", 0) > 0, "source": source}


def _combined_sequence_gaps(directory):
    sequences = []
    for name in ("decoded.jsonl", "pulses.jsonl"):
        path = directory / name
        if not path.is_file():
            continue
        with path.open(encoding="utf-8") as stream:
            sequences.extend(json.loads(line)["seq"] for line in stream if line.strip())
    sequences.sort()
    if any(right == left for left, right in zip(sequences, sequences[1:])):
        raise CaptureError("A sequence number appears in more than one evidence stream")
    return [{"after_seq": left, "before_seq": right, "missing": right - left - 1}
            for left, right in zip(sequences, sequences[1:]) if right > left + 1]


def export_bundle(collector, session_id, output_root, firmware_status, field_notes=None):
    if not SESSION_RE.fullmatch(session_id):
        raise CaptureError("Invalid session identifier")
    listing = collector.command("capture_list", timeout=10)
    session = next((entry for entry in listing.get("sessions", [])
                    if entry.get("session_id") == session_id), None)
    if session is None:
        raise CaptureError("Session is not available in device storage")
    output_root.mkdir(parents=True, exist_ok=True)
    final = output_root / session_id
    partial = output_root / (session_id + ".partial")
    if final.exists() or partial.exists():
        raise CaptureError(f"Refusing to overwrite {final} or {partial}")
    partial.mkdir()
    try:
        files = {}
        decoded = _write_stream(collector, partial, session_id, "decoded", session["decoded_records"])
        if decoded is None:
            raise CaptureError("Session has no decoded evidence records")
        files[decoded["file"]] = {key: decoded[key] for key in ("bytes", "sha256")}
        pulses = _write_stream(collector, partial, session_id, "pulses", session["pulse_records"])
        if pulses:
            files[pulses["file"]] = {key: pulses[key] for key in ("bytes", "sha256")}
        combined_gaps = _combined_sequence_gaps(partial)
        if combined_gaps:
            loss_note = "Combined event sequence gaps are reported; no missing records were synthesized."
        else:
            loss_note = "No combined event sequence gaps were observed in the stored streams."
        session_record = None
        end_record = None
        with (partial / "decoded.jsonl").open(encoding="utf-8") as stored:
            for line in stored:
                if not line.strip():
                    continue
                item = json.loads(line)
                if item.get("event") == "session":
                    session_record = item
                elif item.get("event") == "end":
                    end_record = item
        session_diagnostics = (end_record.get("capture_diagnostics")
                               if isinstance(end_record, dict) else None)
        if not isinstance(session_diagnostics, dict):
            session_diagnostics = None
        diagnostics = {
            "capture_session_diagnostics": session_diagnostics,
            "transport": {"malformed_lines": collector.malformed_lines,
                          "oversized_lines": collector.oversized_lines,
                          "unmatched_events": collector.noise_events},
            "decoded": {key: decoded[key] for key in ("records", "events", "ended")},
            "pulses": ({key: pulses[key] for key in ("records", "events", "ended")}
                       if pulses else None),
            "combined_sequence_gaps": combined_gaps,
            "storage_session": session,
        }
        diagnostics_path = partial / "diagnostics.json"
        diagnostics_bytes = (json.dumps(diagnostics, indent=2, sort_keys=True) + "\n").encode()
        diagnostics_path.write_bytes(diagnostics_bytes)
        files["diagnostics.json"] = {"bytes": len(diagnostics_bytes),
                                     "sha256": hashlib.sha256(diagnostics_bytes).hexdigest()}
        manifest = {
            "schema": 1, "session_id": session_id, "source": decoded["source"],
            "created_utc": datetime.now(timezone.utc).isoformat(),
            "firmware": session_record.get("firmware") if isinstance(session_record, dict) else None,
            "board": session_record.get("board") if isinstance(session_record, dict) else None,
            "profile_id": next((json.loads(line)["profile_id"] for line in
                                 (partial / "decoded.jsonl").read_text(encoding="utf-8").splitlines()
                                 if line), "unknown"),
            "files": files,
            "field_notes": _clean_field_notes(field_notes or {}),
            "loss_totals": {
                "decoder_truncation_drops": _session_loss(session_diagnostics, "decoder_truncation_drops"),
                "queue_full_drops": _session_loss(session_diagnostics, "queue_full_drops"),
                "oversize_drops": _session_loss(session_diagnostics, "oversize_drops"),
                "invalid_json_drops": _session_loss(session_diagnostics, "invalid_json_drops"),
                "storage_drops": _session_loss(session_diagnostics, "storage_drops"),
                "transport_drops": _session_loss(session_diagnostics, "transport_drops"),
                "receiver_drops": _session_loss(session_diagnostics, "receiver_drops"),
                "raw_callback_drops": _session_loss(session_diagnostics, "raw_callback_drops"),
                "raw_callback_skips": _session_loss(session_diagnostics, "raw_callback_skips"),
                "raw_pulse_truncated_records": sum(
                    1 for line in (partial / "pulses.jsonl").read_text(encoding="utf-8").splitlines()
                    if line and json.loads(line).get("truncated") is True) if pulses else 0,
                "host_malformed_lines": collector.malformed_lines,
                "host_oversized_lines": collector.oversized_lines,
                "host_unmatched_events": collector.noise_events,
            },
            "record_counts": {"decoded": decoded["records"],
                              "pulses": pulses["records"] if pulses else 0},
            "interrupted": not decoded["ended"] or bool(session.get("interrupted")),
            "sequence_loss": bool(combined_gaps),
            "privacy_review_required": True,
            "submission": "manual only; inspect neighbor identifiers and timing before sharing",
            "reviewed_derivative": None,
        }
        (partial / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (partial / "README.txt").write_text(
            "WXLink Connect local capture bundle\n\n"
            "Inspect this evidence before sharing. Packet payloads may include nearby device identifiers and timing.\n"
            f"Source: {decoded['source']}. Synthetic and replay data are not live evidence.\n"
            "This bundle contains no credentials or NVS dump. Unknown native fields may identify nearby devices.\n"
            "Submission is manual; explicit privacy review is required before any sharing.\n"
            f"{loss_note}\n", encoding="utf-8")
        _validate_bundle(partial, session_id, manifest)
        partial.rename(final)
        return final
    except Exception:
        raise


def _nonnegative_int(value):
    return value if isinstance(value, int) and not isinstance(value, bool) and value >= 0 else 0


def _session_loss(diagnostics, key):
    if not isinstance(diagnostics, dict):
        return None
    value = diagnostics.get(key)
    return value if isinstance(value, int) and not isinstance(value, bool) and value >= 0 else None


FIELD_NOTE_LIMITS = {
    "station_make": 80, "station_model": 120, "region": 80, "antenna": 120,
    "supply": 80, "console_comparison": 300, "observation_notes": 500,
}


def _clean_field_notes(notes):
    cleaned = {}
    for key, limit in FIELD_NOTE_LIMITS.items():
        value = notes.get(key, "")
        if value is None:
            value = ""
        if not isinstance(value, str):
            raise CaptureError(f"Field note {key} must be text")
        value = "".join(ch for ch in value.strip() if ch >= " " and ch != "\x7f")
        if len(value) > limit:
            raise CaptureError(f"Field note {key} exceeds {limit} characters")
        cleaned[key] = value
        if re.search(r"(?i)(password|passphrase|api[_ -]?key|access[_ -]?token|ssid\s*=|latitude|longitude|\b\d{1,3}\.\d{4,}\s*[, ]\s*-?\d{1,3}\.\d{4,})", value):
            raise CaptureError(f"Field note {key} appears to contain credentials or precise location")
    return cleaned


SENSITIVE_KEY_PARTS = ("password", "passwd", "secret", "credential", "api_key",
                       "token", "nvs", "flash_dump", "mac_address", "ip_address")
ALLOWED_BUNDLE_FILES = {"decoded.jsonl", "pulses.jsonl", "diagnostics.json"}
DEFAULT_REDACTED_NATIVE_KEYS = ("id", "device_id", "station_id", "sensor_id", "serial",
                                "serial_number", "mac", "mac_address")


def _assert_no_private_diagnostics(value, path="diagnostics"):
    if isinstance(value, dict):
        for key, child in value.items():
            lowered = str(key).lower()
            if any(part in lowered for part in SENSITIVE_KEY_PARTS):
                raise CaptureError(f"Private configuration field is not allowed in {path}.{key}")
            _assert_no_private_diagnostics(child, f"{path}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _assert_no_private_diagnostics(child, f"{path}[{index}]")


def _validate_bundle(directory, session_id, manifest):
    required = {"schema", "session_id", "source", "profile_id", "files", "field_notes",
                "loss_totals", "record_counts", "interrupted", "sequence_loss",
                "privacy_review_required", "submission", "reviewed_derivative"}
    if not isinstance(manifest, dict) or not required.issubset(manifest):
        raise CaptureError("Bundle manifest is missing required evidence metadata")
    if (not isinstance(manifest["schema"], int) or isinstance(manifest["schema"], bool) or manifest["schema"] != 1 or
            manifest["session_id"] != session_id or not SESSION_RE.fullmatch(session_id)):
        raise CaptureError("Bundle schema or session identity is invalid")
    if manifest["source"] not in ("live", "synthetic", "replay") or not manifest["privacy_review_required"]:
        raise CaptureError("Bundle source or privacy marker is invalid")
    if manifest.get("submission") != "manual only; inspect neighbor identifiers and timing before sharing":
        raise CaptureError("Bundle must require manual privacy review")
    _assert_no_private_diagnostics(manifest, "manifest")
    if not isinstance(manifest["files"], dict) or set(manifest["files"]) - ALLOWED_BUNDLE_FILES or "decoded.jsonl" not in manifest["files"] or "diagnostics.json" not in manifest["files"]:
        raise CaptureError("Bundle payload file list is invalid")
    session_loss_keys = {"decoder_truncation_drops", "queue_full_drops", "oversize_drops",
                         "invalid_json_drops", "storage_drops", "transport_drops",
                         "receiver_drops", "raw_callback_drops", "raw_callback_skips"}
    required_loss_keys = {"decoder_truncation_drops", "queue_full_drops", "storage_drops",
                          "transport_drops", "raw_pulse_truncated_records", "host_malformed_lines",
                          "host_oversized_lines", "host_unmatched_events"}
    if not isinstance(manifest["loss_totals"], dict) or not required_loss_keys.issubset(manifest["loss_totals"]):
        raise CaptureError("Manifest must report every capture and transport loss total")
    for key, value in manifest["loss_totals"].items():
        if not isinstance(key, str):
            raise CaptureError("Loss total keys must be text")
        if value is None and key in session_loss_keys:
            continue
        if not isinstance(value, int) or isinstance(value, bool) or value < 0:
            raise CaptureError("Known loss totals must be nonnegative integers; unavailable session totals must be null")
    for key in ("firmware", "board"):
        if manifest.get(key) is not None and (not isinstance(manifest[key], str) or not manifest[key]):
            raise CaptureError(f"Manifest {key} must be text from the recorded session or null")
    for name, metadata in manifest["files"].items():
        if (not isinstance(metadata, dict) or set(metadata) != {"bytes", "sha256"} or
                not isinstance(metadata.get("bytes"), int) or isinstance(metadata.get("bytes"), bool) or metadata["bytes"] < 0 or
                not isinstance(metadata.get("sha256"), str) or not re.fullmatch(r"[0-9a-f]{64}", metadata["sha256"])):
            raise CaptureError(f"Invalid checksum entry for {name}")
        path = directory / name
        if path.parent != directory or path.is_symlink() or not path.is_file():
            raise CaptureError(f"Bundle payload is missing or unsafe: {name}")
        data = path.read_bytes()
        if len(data) != metadata["bytes"] or hashlib.sha256(data).hexdigest() != metadata["sha256"]:
            raise CaptureError(f"Checksum mismatch for {name}")
    try:
        diagnostics = json.loads((directory / "diagnostics.json").read_text(encoding="utf-8"))
    except (OSError, ValueError, UnicodeDecodeError) as error:
        raise CaptureError("Diagnostics JSON is malformed") from error
    if not isinstance(diagnostics, dict):
        raise CaptureError("Diagnostics document must be an object")
    _assert_no_private_diagnostics(diagnostics)
    session_diagnostics = diagnostics.get("capture_session_diagnostics")
    if "capture_session_diagnostics" in diagnostics:
        for key in session_loss_keys:
            expected = session_diagnostics.get(key) if isinstance(session_diagnostics, dict) else None
            if manifest["loss_totals"].get(key) != expected:
                raise CaptureError(f"Manifest {key} does not match stored session diagnostics")
    host_transport = diagnostics.get("transport", {})
    if isinstance(host_transport, dict):
        for manifest_key, transport_key in (("host_malformed_lines", "malformed_lines"),
                                            ("host_oversized_lines", "oversized_lines"),
                                            ("host_unmatched_events", "unmatched_events")):
            if transport_key in host_transport and manifest["loss_totals"].get(manifest_key) != host_transport[transport_key]:
                raise CaptureError(f"Manifest {manifest_key} does not match collector diagnostics")
    if not isinstance(manifest["field_notes"], dict):
        raise CaptureError("Manifest field notes must be an object")
    notes = _clean_field_notes(manifest["field_notes"])
    if notes != manifest["field_notes"]:
        raise CaptureError("Manifest field notes are not normalized")
    counts = manifest["record_counts"]
    if not isinstance(counts, dict) or any(not isinstance(counts.get(key), int) or isinstance(counts.get(key), bool) or counts[key] < 0 for key in ("decoded", "pulses")):
        raise CaptureError("Manifest record counts are invalid")

    stream_summary = {}
    stream_sequences = {}
    recorded_firmware = recorded_board = None
    stored_session_diagnostics = None
    for name, expected_kind in (("decoded.jsonl", "decoded"), ("pulses.jsonl", "pulses")):
        if name not in manifest["files"]:
            if expected_kind == "pulses" and counts["pulses"] == 0:
                continue
            raise CaptureError(f"Manifest omits required {name}")
        sequences, event_counts = [], {}
        source = None
        first = True
        first_profile_id = None
        ended = False
        truncated_records = 0
        path = directory / name
        with path.open("rb") as raw_stream:
            for raw in raw_stream:
                if not raw.endswith(b"\n") or len(raw) > MAX_LINE_BYTES:
                    raise CaptureError(f"{name} contains a partial or oversized JSONL line")
                try:
                    line = raw.decode("utf-8")
                    item = json.loads(line)
                except (UnicodeDecodeError, ValueError) as error:
                    raise CaptureError(f"{name} contains malformed JSONL") from error
                ok, reason = valid_envelope(item, session_id)
                if not ok:
                    raise CaptureError(f"{name} validation failed: {reason}")
                if source is None:
                    source = item["source"]
                if item["source"] != source or source != manifest["source"]:
                    raise CaptureError("Manifest source does not match all stream records")
                if first and expected_kind == "decoded":
                    if item["event"] != "session" or item["seq"] != 0:
                        raise CaptureError("Decoded stream must begin with its session record")
                    if "decoder_local_receiver_revision" in item:
                        revision = item["decoder_local_receiver_revision"]
                        if not isinstance(revision, str) or not revision or len(revision) > 128:
                            raise CaptureError("Local receiver revision is invalid")
                    if item["source"] == "live":
                        required_profile = ("decoder_library", "decoder_revision", "profile_revision",
                                            "requested_center_frequency_mhz", "requested_modulation",
                                            "requested_rx_bandwidth_khz", "requested_rssi_margin_db",
                                            "radio_initialization_state", "radio_readback_verified", "decoder_symbols")
                        if any(key not in item for key in required_profile):
                            raise CaptureError("Live session record is missing RF/decoder provenance")
                        if (not isinstance(item["decoder_library"], str) or not item["decoder_library"] or
                                not isinstance(item["decoder_revision"], str) or not item["decoder_revision"] or
                                not isinstance(item["profile_revision"], int) or isinstance(item["profile_revision"], bool) or item["profile_revision"] < 1 or
                                not isinstance(item["requested_center_frequency_mhz"], (int, float)) or isinstance(item["requested_center_frequency_mhz"], bool) or item["requested_center_frequency_mhz"] <= 0 or
                                item["requested_modulation"] not in ("OOK", "FSK") or
                                item["radio_initialization_state"] not in ("init_accepted_readback_unverified", "initialization_failed", "not_initialized") or
                                not isinstance(item["requested_rx_bandwidth_khz"], (int, float)) or isinstance(item["requested_rx_bandwidth_khz"], bool) or item["requested_rx_bandwidth_khz"] <= 0 or
                                not isinstance(item["requested_rssi_margin_db"], int) or isinstance(item["requested_rssi_margin_db"], bool) or item["requested_rssi_margin_db"] < 0 or
                                not isinstance(item["radio_readback_verified"], bool) or
                                not isinstance(item["decoder_symbols"], list) or
                                any(not isinstance(symbol, str) or not symbol for symbol in item["decoder_symbols"]) or
                                len(set(item["decoder_symbols"])) != len(item["decoder_symbols"])):
                            raise CaptureError("Live session RF/decoder provenance is invalid")
                        if ("firmware" in item or "board" in item) and (
                                not isinstance(item.get("firmware"), str) or not item["firmware"] or
                                not isinstance(item.get("board"), str) or not item["board"]):
                            raise CaptureError("Live session firmware/board provenance is incomplete")
                    recorded_firmware, recorded_board = item.get("firmware"), item.get("board")
                    first_profile_id = item["profile_id"]
                first = False
                if expected_kind == "pulses" and item["event"] != "pulse":
                    raise CaptureError("Pulse stream contains a non-pulse event")
                if expected_kind == "decoded" and item["event"] == "pulse":
                    raise CaptureError("Decoded stream contains a pulse event")
                if sequences and item["seq"] <= sequences[-1]:
                    raise CaptureError(f"{name} sequence ordering failed")
                sequences.append(item["seq"])
                event_counts[item["event"]] = event_counts.get(item["event"], 0) + 1
                if item["event"] == "end":
                    ended = True
                    end_diagnostics = item.get("capture_diagnostics")
                    if end_diagnostics is not None:
                        if not isinstance(end_diagnostics, dict):
                            raise CaptureError("Stored capture diagnostics must be an object")
                        stored_session_diagnostics = end_diagnostics
                if item["event"] == "pulse" and item.get("truncated"):
                    truncated_records += 1
        if not sequences:
            raise CaptureError(f"{name} is empty")
        if expected_kind == "decoded" and first_profile_id != manifest["profile_id"]:
            raise CaptureError("Manifest profile does not match the session-start record")
        if expected_kind == "decoded" and (("firmware" in manifest and manifest["firmware"] != recorded_firmware) or
                                            ("board" in manifest and manifest["board"] != recorded_board)):
            raise CaptureError("Manifest firmware/board must come from the session-start record")
        expected_count = counts["decoded"] if expected_kind == "decoded" else counts["pulses"]
        if len(sequences) != expected_count:
            raise CaptureError(f"Manifest {expected_kind} count does not match records")
        stream_sequences[expected_kind] = sequences
        stream_summary[expected_kind] = {"records": len(sequences), "events": event_counts,
                                         "ended": ended, "truncated": truncated_records,
                                         "sequence_gaps": any(b > a + 1 for a, b in zip(sequences, sequences[1:]))}
    if (manifest.get("reviewed_derivative") is None and
            manifest["loss_totals"]["raw_pulse_truncated_records"] != stream_summary.get("pulses", {}).get("truncated", 0)):
        raise CaptureError("Manifest pulse truncation total disagrees with pulse records")
    if "capture_session_diagnostics" in diagnostics:
        for key in session_loss_keys:
            expected = stored_session_diagnostics.get(key) if isinstance(stored_session_diagnostics, dict) else None
            if manifest["loss_totals"].get(key) != expected:
                raise CaptureError(f"Manifest {key} does not match the stored end record")
    if manifest["interrupted"] != (not stream_summary["decoded"]["ended"]):
        raise CaptureError("Manifest interrupted flag disagrees with end record")
    if not isinstance(manifest["sequence_loss"], bool):
        raise CaptureError("Manifest sequence-loss marker is invalid")
    derivative = manifest.get("reviewed_derivative")
    if derivative is None:
        combined = sorted(stream_sequences.get("decoded", []) + stream_sequences.get("pulses", []))
        if len(set(combined)) != len(combined):
            raise CaptureError("Event sequence number appears in multiple streams")
        if manifest["sequence_loss"] != any(b > a + 1 for a, b in zip(combined, combined[1:])):
            raise CaptureError("Manifest sequence-loss flag disagrees with combined JSONL streams")
    else:
        allowed_transforms = ("selected_device_filter", "all_device_review",
                              "selected_device_filter_and_native_key_redaction",
                              "all_device_review_and_native_key_redaction")
        if not isinstance(derivative, dict) or derivative.get("transformation") not in allowed_transforms:
            raise CaptureError("Reviewed derivative metadata is invalid")
        if derivative.get("manual_public_sharing_choice") not in ("keep_local", "review_for_public_sharing"):
            raise CaptureError("Reviewed derivative is missing an explicit sharing choice")
        if not isinstance(derivative.get("native_payloads_modified"), bool):
            raise CaptureError("Reviewed derivative must state whether native payloads changed")
        redaction = derivative.get("redaction")
        if derivative["native_payloads_modified"]:
            if not isinstance(redaction, dict) or redaction.get("keys") != list(DEFAULT_REDACTED_NATIVE_KEYS) or redaction.get("replacement") != "[REDACTED]":
                raise CaptureError("Native payload redaction transformation is not documented")
    return stream_summary


def _packet_identities(directory, session_id):
    identities = {}
    with (directory / "decoded.jsonl").open(encoding="utf-8") as stream:
        for line in stream:
            record = json.loads(line)
            if record["event"] != "packet":
                continue
            try:
                native = json.loads(record["native_json"])
            except ValueError:
                continue
            def scalar(value):
                return str(value) if isinstance(value, (str, int, float)) and not isinstance(value, bool) else ""
            identity = {"protocol": scalar(native.get("protocol")),
                        "model": scalar(native.get("model")),
                        "id": scalar(native.get("id")),
                        "channel": scalar(native.get("channel"))}
            key = tuple(identity[name] for name in ("protocol", "model", "id", "channel"))
            identities.setdefault(key, {"identity": identity, "packets": 0})["packets"] += 1
    return identities


def _redact_native(value):
    if isinstance(value, dict):
        for key in list(value):
            if str(key).lower() in DEFAULT_REDACTED_NATIVE_KEYS:
                value[key] = "[REDACTED]"
            else:
                _redact_native(value[key])
    elif isinstance(value, list):
        for child in value:
            _redact_native(child)
    return value


def _review_derivative(source, output, selection, sharing_choice="keep_local", redact=False):
    if sharing_choice not in ("keep_local", "review_for_public_sharing"):
        raise CaptureError("Public sharing choice is invalid")
    manifest_path = source / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, UnicodeDecodeError) as error:
        raise CaptureError("Bundle manifest is missing or malformed") from error
    _validate_bundle(source, manifest.get("session_id", ""), manifest)
    identities = _packet_identities(source, manifest["session_id"])
    partial = output.with_name(output.name + ".partial")
    if output.exists() or partial.exists():
        raise CaptureError(f"Refusing to overwrite reviewed derivative {output} or {partial}")
    if selection != "all":
        if selection not in identities:
            raise CaptureError("Selected identity is not present in the source bundle")
    partial.mkdir(parents=True)
    output = partial
    try:
        original_manifest = json.loads(json.dumps(manifest))
        decoded = source / "decoded.jsonl"
        target_decoded = output / "decoded.jsonl"
        excluded = 0
        kept = 0
        with decoded.open(encoding="utf-8") as in_stream, target_decoded.open("x", encoding="utf-8", newline="") as out_stream:
            for line in in_stream:
                record = json.loads(line)
                if selection != "all" and record["event"] == "packet":
                    native = json.loads(record["native_json"])
                    def scalar(value):
                        return str(value) if isinstance(value, (str, int, float)) and not isinstance(value, bool) else ""
                    key = tuple(scalar(native.get(field)) for field in ("protocol", "model", "id", "channel"))
                    if key != selection:
                        excluded += 1
                        continue
                    kept += 1
                if redact and record["event"] == "packet":
                    native = json.loads(record["native_json"])
                    record["native_json"] = json.dumps(_redact_native(native), ensure_ascii=False, separators=(",", ":"))
                out_stream.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n")
        # Pulses have no trustworthy packet/device correlation and are omitted from a selected-device derivative.
        if selection == "all" and "pulses.jsonl" in manifest["files"]:
            (output / "pulses.jsonl").write_bytes((source / "pulses.jsonl").read_bytes())
        diagnostics = json.loads((source / "diagnostics.json").read_text(encoding="utf-8"))
        diagnostics["review_derivative"] = {"selection": "all_devices" if selection == "all" else "selected_device",
                                             "excluded_packet_records": excluded}
        diagnostics_bytes = (json.dumps(diagnostics, indent=2, sort_keys=True) + "\n").encode()
        (output / "diagnostics.json").write_bytes(diagnostics_bytes)
        files = {}
        for name in ("decoded.jsonl", "pulses.jsonl", "diagnostics.json"):
            path = output / name
            if path.is_file():
                data = path.read_bytes()
                files[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        decoded_records = sum(1 for line in (output / "decoded.jsonl").read_text(encoding="utf-8").splitlines() if line)
        pulse_records = sum(1 for line in (output / "pulses.jsonl").read_text(encoding="utf-8").splitlines() if line) if (output / "pulses.jsonl").exists() else 0
        manifest["files"] = files
        manifest["record_counts"] = {"decoded": decoded_records, "pulses": pulse_records}
        # Intentional packet filtering creates sequence gaps; retain the source loss marker.
        manifest["sequence_loss"] = original_manifest["sequence_loss"]
        manifest["interrupted"] = original_manifest["interrupted"]
        manifest["reviewed_derivative"] = {
            "parent_session_id": original_manifest["session_id"],
            "transformation": (("all_device_review" if selection == "all" else "selected_device_filter") +
                               ("_and_native_key_redaction" if redact else "")),
            "selected_identity": None if selection == "all" else identities[selection]["identity"],
            "excluded_packet_records": excluded,
            "retained_packet_records": kept if selection != "all" else identities_total(identities),
            "pulses_included": selection == "all" and "pulses.jsonl" in files,
            "native_payloads_modified": redact,
            "redaction": ({"keys": list(DEFAULT_REDACTED_NATIVE_KEYS), "replacement": "[REDACTED]",
                           "scope": "recursive keys in packet native_json objects"} if redact else None),
            "manual_public_sharing_choice": sharing_choice,
        }
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (output / "README.txt").write_text(
            "WXLink Connect reviewed derivative\n\n"
            "This is a new derivative; the source bundle remains unchanged.\n"
            "Selected-device filtering cannot guarantee other identifiers are absent from native payload fields.\n"
            + ("Common ID keys in packet native_json objects were replaced with [REDACTED]; unknown key names may remain.\n" if redact else "Native payload strings were preserved unchanged.\n")
            + "Pulse records are omitted from selected-device derivatives because no packet correlation exists.\n"
            + f"Disposition: {sharing_choice}. This file is not submitted anywhere.\n"
            "Review it manually before sharing.\n", encoding="utf-8")
        _validate_bundle(output, manifest["session_id"], manifest)
        output.rename(output.with_name(output.name[:-len(".partial")]))
        return output.with_name(output.name[:-len(".partial")])
    except Exception:
        raise


def identities_total(identities):
    return sum(entry["packets"] for entry in identities.values())


def review_bundle(source, output=None, input_fn=input, output_fn=print):
    source = Path(source)
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    _validate_bundle(source, manifest.get("session_id", ""), manifest)
    identities = _packet_identities(source, manifest["session_id"])
    ordered = list(identities.items())
    output_fn("Review source bundle. Native payloads may contain identifiers not listed here.")
    for index, (_, value) in enumerate(ordered, 1):
        ident = value["identity"]
        label = " / ".join(f"{key}={ident[key] or '(unknown)'}" for key in ("protocol", "model", "id", "channel"))
        output_fn(f"{index}: {label} · {value['packets']} packets")
    output_fn(f"a: all devices ({sum(v['packets'] for v in identities.values())} packets)")
    notes = manifest.get("field_notes", {})
    if any(notes.values()):
        output_fn("User-entered field notes:")
        for key, value in notes.items():
            if value:
                output_fn(f"  {key}: {value}")
    choice = input_fn("Choose a device number or 'a' for all devices: ").strip().lower()
    if choice == "a":
        selection = "all"
    else:
        try:
            selected_index = int(choice) - 1
            selection = ordered[selected_index][0]
        except (ValueError, IndexError):
            raise CaptureError("Privacy review selection is invalid")
    output_fn("This creates a new derivative; the original downloaded bundle stays unchanged.")
    output_fn("Selected-device filtering can leave other identifiers in unknown native fields.")
    output_fn("No files are submitted or uploaded by this tool.")
    disposition = input_fn("Choose disposition: 'local' or 'public-review' (manual only): ").strip().lower()
    if disposition not in ("local", "public-review"):
        raise CaptureError("A local-only or explicit manual public-review choice is required")
    sharing_choice = "keep_local" if disposition == "local" else "review_for_public_sharing"
    redact_answer = input_fn("Create a redacted derivative replacing common ID fields? [y/N]: ").strip().lower()
    if redact_answer not in ("y", "n"):
        raise CaptureError("Choose whether to redact common ID fields")
    redact = redact_answer == "y"
    if redact and input_fn(f"Type REDACT {manifest['session_id']} to confirm native JSON changes: ").strip() != f"REDACT {manifest['session_id']}":
        raise CaptureError("Native payload redaction was not confirmed")
    expected = f"REVIEW {manifest['session_id']}"
    if input_fn(f"Type {expected} to create the reviewed derivative: ").strip() != expected:
        raise CaptureError("Privacy review was not confirmed; no derivative was created")
    if output is None:
        output = source.with_name(source.name + "-reviewed")
    result = _review_derivative(source, Path(output), selection, sharing_choice, redact)
    output_fn(f"Created local reviewed derivative: {result}")
    return result


def select_port(requested):
    import serial
    if requested:
        return requested
    from serial.tools import list_ports
    ports = list(list_ports.comports())
    if not ports:
        raise CaptureError("No serial ports found; connect the capture device and retry with --port")
    for index, item in enumerate(ports, 1):
        print(f"{index}: {item.device} — {item.description}")
    try:
        selection = int(input("Select the WXLink Connect USB port: "))
    except (ValueError, EOFError) as error:
        raise CaptureError("A port selection is required") from error
    if selection < 1 or selection > len(ports):
        raise CaptureError("Port selection is outside the listed range")
    return ports[selection - 1].device


def run_capture(port, args):
    import serial
    device = serial.Serial(port=None, baudrate=115200, timeout=0.5,
                           write_timeout=5, exclusive=True)
    device.dtr = True
    device.rts = True
    device.port = port
    try:
        device.open()
    except (serial.SerialException, OSError) as error:
        raise CaptureError(f"Cannot open {port} exclusively: {error}") from error
    with device:
        collector = Collector(device)
        firmware_status, capture_status = collector.sync()
        if args.delete_session_id:
            session_id = args.delete_session_id
            if not SESSION_RE.fullmatch(session_id):
                raise CaptureError("Invalid --delete-session-id")
            answer = input(f"Type {session_id} to permanently delete that device session: ")
            if answer != session_id:
                raise CaptureError("Session deletion was not confirmed; nothing was deleted")
            reply = collector.command("capture_delete", session_id=session_id)
            if not reply.get("ok"):
                raise CaptureError("Device refused deletion (session may be active or unavailable)")
            print(f"Deleted device session {session_id}; any downloaded bundle remains on this computer")
            return None
        if args.session_id:
            session_id = args.session_id
            if not SESSION_RE.fullmatch(session_id):
                raise CaptureError("Invalid --session-id")
            if capture_status.get("active") and capture_status.get("session_id") == session_id:
                try:
                    collector.command("capture_stop", timeout=10)
                except (OSError, serial.SerialException, TimeoutError, CaptureError):
                    raise CaptureError("Could not stop the requested active session before export")
            return export_bundle(collector, session_id, args.output, firmware_status, args.field_notes)
        if capture_status.get("active"):
            session_id = capture_status.get("session_id")
            print(f"Resuming active session {session_id}", file=sys.stderr)
        else:
            reply = collector.command("capture_start", timeout=10)
            session_id = reply.get("session_id")
            if not SESSION_RE.fullmatch(session_id or ""):
                raise CaptureError("Firmware did not provide a valid session ID")
        started = time.monotonic()
        heartbeat = started
        try:
            while args.duration <= 0 or time.monotonic() - started < args.duration:
                time.sleep(0.25)
                if time.monotonic() - heartbeat >= 5:
                    collector.keepalive(session_id)
                    heartbeat = time.monotonic()
                if args.raw_window_seconds and time.monotonic() - started < args.duration:
                    result = collector.command("raw_window_start", duration_ms=args.raw_window_seconds * 1000)
                    if result.get("event") != "raw_window":
                        raise CaptureError("Raw pulse window was not acknowledged")
                    args.raw_window_seconds = 0
        except KeyboardInterrupt:
            pass
        finally:
            try:
                collector.command("capture_stop", timeout=10)
            except (OSError, serial.SerialException, TimeoutError, CaptureError):
                # Device-side lease closes the session after 20 seconds; a later
                # invocation can discover and export the interrupted session.
                pass
        return export_bundle(collector, session_id, args.output, firmware_status, args.field_notes)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="USB serial port (otherwise choose from enumerated ports)")
    parser.add_argument("--output", type=Path, default=Path("wxconnect-captures"))
    parser.add_argument("--duration", type=int, default=0, help="capture seconds; 0 runs until Ctrl-C")
    parser.add_argument("--session-id", help="export a previously saved session after reconnecting")
    parser.add_argument("--delete-session-id", help="delete a saved device session after typing its ID")
    parser.add_argument("--raw-window-seconds", type=int, choices=range(1, 11),
                        help="request one deliberate raw-pulse window after capture starts")
    parser.add_argument("--review-bundle", type=Path, help="create a privacy-reviewed local derivative without opening USB")
    parser.add_argument("--validate-bundle", type=Path, help="validate an existing local bundle without opening USB")
    parser.add_argument("--review-output", type=Path, help="destination for --review-bundle derivative")
    for key, label in (("station_make", "station make"), ("station_model", "station model"),
                       ("region", "region (no address)"), ("antenna", "antenna"),
                       ("supply", "device supply"), ("console_comparison", "console comparison"),
                       ("observation_notes", "observation notes")):
        parser.add_argument("--" + key.replace("_", "-"), dest=key, help=f"user-entered {label}")
    args = parser.parse_args(argv)
    if args.duration < 0:
        parser.error("--duration must be zero or greater")
    if args.session_id and args.delete_session_id:
        parser.error("--session-id and --delete-session-id cannot be combined")
    if args.review_bundle and args.validate_bundle:
        parser.error("--review-bundle and --validate-bundle cannot be combined")
    if args.validate_bundle:
        if args.port or args.session_id or args.delete_session_id or args.review_output:
            parser.error("--validate-bundle cannot be combined with USB capture or review options")
        try:
            manifest = json.loads((args.validate_bundle / "manifest.json").read_text(encoding="utf-8"))
            summary = _validate_bundle(args.validate_bundle, manifest.get("session_id", ""), manifest)
            print(f"PASS: validated {manifest['source']} bundle {manifest['session_id']} ({summary['decoded']['records']} decoded records)")
        except (CaptureError, OSError, ValueError, KeyError) as error:
            print(f"bundle validation failed: {error}", file=sys.stderr)
            return 2
        return 0
    if args.review_bundle:
        if args.port or args.session_id or args.delete_session_id:
            parser.error("--review-bundle cannot be combined with USB capture or device session options")
        try:
            review_bundle(args.review_bundle, args.review_output)
        except (CaptureError, OSError, ValueError) as error:
            print(f"privacy review failed: {error}", file=sys.stderr)
            return 2
        return 0
    if args.review_output:
        parser.error("--review-output requires --review-bundle")
    args.field_notes = {key: getattr(args, key) or "" for key in FIELD_NOTE_LIMITS}
    try:
        run_capture(select_port(args.port), args)
    except (CaptureError, TimeoutError, OSError) as error:
        print(f"capture failed: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
