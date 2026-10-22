#!/usr/bin/env python3
"""Check compiled stack budgets and critical Connect runtime wiring."""
from pathlib import Path
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware/.pio/build/panellan_sc05_x"
MAIN_SU = BUILD / "src/main.cpp.su"
MIGRATION_SU = BUILD / "src/connect_migration.cpp.su"
ELF = BUILD / "firmware.elf"
if not MAIN_SU.is_file() or not ELF.is_file():
    raise SystemExit("FAIL: build SC05_X before checking Connect runtime stack usage")
if not MIGRATION_SU.is_file():
    raise SystemExit("FAIL: migration stack-usage data is missing; rebuild with -fstack-usage")

source = (ROOT / "firmware/src/main.cpp").read_text()
if "SET_LOOP_TASK_STACK_SIZE(24 * 1024)" not in source:
    raise SystemExit("FAIL: loop task stack must be explicitly sized to 24 KiB")

usage = {}
for line in MAIN_SU.read_text().splitlines():
    match = re.search(r"\b(\d+)\s+(?:static|dynamic)(?:\s|$)", line)
    if match:
        usage[line.split("\t", 1)[0]] = int(match.group(1))

limits = {
    "void loop()": 1024,
    "liveReceive()": 7168,
    "drainPulseQueue()": 1024,
    "drainCaptureQueue()": 1024,
    "writeCaptureRecord(const wxconnect::capture::Record&)": 1024,
    "command(const char*)": 7168,
    "captureStartRoute()": 2048,
    "captureStopRoute()": 2048,
    "captureHeartbeatRoute()": 2048,
    "connectStatusRoute()": 7168,
    "sessionsRoute()": 5120,
}
for function, limit in limits.items():
    entries = [(name, size) for name, size in usage.items() if function in name]
    if not entries:
        raise SystemExit(f"FAIL: compiled stack usage missing {function}")
    worst = max(size for _, size in entries)
    if worst > limit:
        raise SystemExit(f"FAIL: {function} stack frame {worst} exceeds {limit} bytes")

radio_header = (ROOT / "firmware/include/radio_receiver.h").read_text()
live = source.split("void liveReceive(){", 1)[1].split("\n}", 1)[0]
if "bool pop(char (&output)[kDecodedBufferBytes]" not in radio_header:
    raise SystemExit("FAIL: decoder queue API must preserve array capacity in its type")
if "radio_receiver::pop(packet,received_length,truncated)" not in live or "sizeof(packet)" in live:
    raise SystemExit("FAIL: live receive may pass pointer size instead of decoder-buffer capacity")

writer = source.split("bool writeCaptureRecord(", 1)[1].split("\n}", 1)[0]
if "appendOrStop" not in writer or "abortForStorageFailure" not in writer or "storage::finish" not in writer:
    raise SystemExit("FAIL: storage append rejection must terminate and close live recording")
if "capture_session.abortForStorageFailure()" not in source or "storage::tick())" not in source:
    raise SystemExit("FAIL: timed flush failure must also terminate capture")

if 'web_server.on("/api/connect/heartbeat",HTTP_POST,captureHeartbeatRoute)' not in source:
    raise SystemExit("FAIL: authenticated browser heartbeat route is not registered")
if "capture_lease.heartbeat(CaptureOwner::Browser,uptimeMs())" not in source:
    raise SystemExit("FAIL: browser heartbeat does not renew the browser-owned lease")
if "setInterval(heartbeat,5000)" not in (ROOT / "firmware/include/setup_page.h").read_text():
    raise SystemExit("FAIL: Connect page does not renew active browser capture")

migration = (ROOT / "firmware/src/connect_migration.cpp").read_text()
if 'xTaskCreatePinnedToCore(worker, "wx-main-transition",\n      16384' not in migration:
    raise SystemExit("FAIL: migration must run in its dedicated 16 KiB worker")
if "setInsecure" in migration or "migration_policy::kManifestUrl" not in migration:
    raise SystemExit("FAIL: migration must use the fixed HTTPS manifest and certificate validation")
migration_usage = {}
for line in MIGRATION_SU.read_text().splitlines():
    match = re.search(r"\b(\d+)\s+(?:static|dynamic)(?:\s|$)", line)
    if match:
        migration_usage[line.split("\t", 1)[0]] = int(match.group(1))
worker_frames = [(name, size) for name, size in migration_usage.items() if "runMigration()" in name]
if not worker_frames or max(size for _, size in worker_frames) > 8192:
    raise SystemExit("FAIL: migration worker stack frame exceeds its reviewed 8 KiB frame budget")

exporter = (ROOT / "tools/connect_capture.py").read_text()
export_function = exporter.split("def export_bundle(", 1)[1].split("\ndef _nonnegative_int", 1)[0]
if 'collector.command("capture_status"' in export_function:
    raise SystemExit("FAIL: historical export reads current-session loss counters")
if '"capture_session_diagnostics"' not in export_function or 'session_record.get("firmware")' not in export_function:
    raise SystemExit("FAIL: export must use persisted session/end metadata")

core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
nm = shutil.which("xtensa-esp32s3-elf-nm")
if not nm:
    matches = list((core / "packages/toolchain-xtensa-esp32s3/bin").glob("xtensa-esp32s3-elf-nm"))
    if matches:
        nm = str(matches[0])
if not nm:
    raise SystemExit("FAIL: Xtensa nm is needed to verify the compiled strong stack-size override")
symbols = subprocess.check_output([nm, "-C", str(ELF)], text=True)
if not re.search(r"\bT\s+getArduinoLoopTaskStackSize\(\)", symbols):
    raise SystemExit("FAIL: firmware ELF does not contain the strong loop-stack override")

# The first device boot left only ~5 KiB heap with 245 KiB static internal RAM.
# Reserve at least ~80 KiB more for networking; PSRAM pools must not regress into BSS.
sized_symbols = subprocess.check_output([nm, "-S", str(ELF)], text=True)
internal_static = sum(int(m.group(1), 16) for line in sized_symbols.splitlines()
    if (m := re.match(r"[0-9a-f]+\s+([0-9a-f]+)\s+[bBdD]\s+", line))
    and 0x3fc80000 <= int(line.split()[0], 16) < 0x3fd00000)
if internal_static > 175 * 1024:
    raise SystemExit(f"FAIL: internal static RAM {internal_static} exceeds 175 KiB networking budget")
print(f"PASS: internal static RAM {internal_static} bytes within 175 KiB budget")

maximum = max(usage.values())
print(f"PASS: runtime wiring and stack budget; largest main.cpp frame {maximum} bytes; migration frame {max(size for _, size in worker_frames)} bytes on a 16384-byte worker; loop task override 24576 bytes")
