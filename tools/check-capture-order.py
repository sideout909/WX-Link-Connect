#!/usr/bin/env python3
"""Check the runtime source keeps native capture ahead of weather filtering."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "firmware/src/main.cpp").read_text()
receiver = (root / "firmware/src/radio_receiver.cpp").read_text()
live = main.split("void liveReceive(){", 1)[1].split("\n}\n", 1)[0]
ordered = [
    "capture_session.enqueueDecoderOutput",
    "wxconnect::preview::parseNative(packet,native_length,d)",
    "capture_candidates.observe",
    "preview_adapter.ingest",
    "station_confirmed",
    "atlas::decode",
    "store->ingest",
]
positions = [live.find(marker) for marker in ordered]
if any(position < 0 for position in positions) or positions != sorted(positions):
    raise SystemExit("FAIL: native capture/candidate delivery must precede Atlas filtering and ingest")

if ("constexpr size_t kMessageSize = kDecodedBufferBytes" not in receiver or
        "constexpr size_t kDecodedBufferBytes = 16384" not in (root / "firmware/include/radio_receiver.h").read_text()):
    raise SystemExit("FAIL: decoder copy buffer no longer covers worst-case escaping for accepted native JSON")
if "memcpy(message.json,json,length+1)" not in receiver or "xQueueSend(queue,&slot,0)" not in receiver:
    raise SystemExit("FAIL: callback data is not copied into a nonblocking bounded queue")
if "Message message{}" in receiver or "Message message;" in receiver:
    raise SystemExit("FAIL: callback allocates a 16 KiB queue item on the decoder task stack")
formatter = (root / "vendor/rtl_433_ESP/src/rtl_433/data.c").read_text()
if "data_print_jsons_checked" not in formatter or "jsons->truncated = 1" not in formatter:
    raise SystemExit("FAIL: JSON formatter does not report output truncation")
if "if(truncated)continue;" not in live or "decoder_truncation_drops" not in main:
    raise SystemExit("FAIL: truncated native output can reach candidates or lacks loss accounting")
if "receiver.setRawPulsesCallback(rawDecoded)" not in receiver:
    raise SystemExit("FAIL: bounded raw-pulse callback is not registered")
if "kRawWindowMinMs = 1000" not in (root / "firmware/include/capture_pulse.h").read_text():
    raise SystemExit("FAIL: raw-pulse window no longer has an explicit minimum/maximum bound")
if "copyFromCallback" not in (root / "firmware/src/radio_receiver.cpp").read_text():
    raise SystemExit("FAIL: raw pulse pointers are not copied by the bounded queue")
if "packet_seq\\\":null" not in (root / "firmware/include/capture_jsonl.h").read_text():
    raise SystemExit("FAIL: pulse records claim correlation without a shared frame identifier")
storage = (root / "firmware/src/capture_storage.cpp").read_text()
if "LittleFS.begin(false)" not in storage or "LittleFS.format" in storage:
    raise SystemExit("FAIL: capture storage must mount without formatting user data")
if "kPoolBytes = 256 * 1024" not in (root / "firmware/include/capture_storage.h").read_text():
    raise SystemExit("FAIL: per-stream storage budgets changed without review")
if "capture_session.active()" not in live:
    raise SystemExit("FAIL: callback queue is not gated by explicit capture session state")
if "wxconnect::preview::parseNative(packet,native_length,d)" not in live:
    raise SystemExit("FAIL: broker preview parse must preserve native packet bytes")
print("PASS: callback copy is bounded and nonblocking; native records reach capture before Atlas filtering")
