#!/usr/bin/env python3
"""Host audit for Connect storage and the compiled application boundary."""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / "firmware"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


platformio = (FW / "platformio.ini").read_text()
main = (FW / "src/main.cpp").read_text()
services = (FW / "src/services.cpp").read_text()
transition = (FW / "src/connect_migration.cpp").read_text()
transition_policy = (FW / "include/connect_migration_policy.h").read_text()
checkpoint = (FW / "src/checkpoint.cpp").read_text()
archive = (FW / "src/archive_storage.cpp").read_text()
setup_page = (FW / "include/setup_page.h").read_text()

require("-<network_jobs.cpp>" in platformio, "production network/exporter source is included")
require("-<firmware_updates.cpp>" in platformio, "production OTA source is included")
require('"/api/connect/switch-to-wx-link"' in main and
        'https://github.com/sideout909/WX-Link/releases/latest/download/' in transition_policy,
        "fixed, deliberate Connect-to-main migration path is missing")
require("update_repo" not in transition and "update_auto" not in transition,
        "migration target can be changed or background-enabled")
require("firmware_updates::" not in main + services, "Connect call sites still reach production OTA")
require("archive_storage::" not in main and "checkpoint::" not in main,
        "automatic weather history persistence/download remains reachable")
require('preferences.begin("wxconnect", false)' in services,
        "Connect Preferences namespace is not isolated")
require('preferences.getString("settings"' in services and
        'WiFi.softAP(current.ap_name.c_str(), config.setup_wifi_key.c_str())' in services and
        'String adminPassword() { return config.setup_key; }' in services,
        "admin authentication is not derived from the isolated Connect settings")
require("preferences.getString(\"settings\"" in services,
        "Connect does not load settings from its own namespace")
require("prefs.begin(\"wxlink\"" not in services and "Preferences.begin(\"wxlink\"" not in services,
        "legacy wxlink Preferences namespace is opened")
require('"/connect/' in checkpoint and '"/connect/' in archive,
        "persistent Connect files are not rooted under /connect/")
require("LittleFS.format()" not in checkpoint + archive,
        "Connect can format a filesystem containing prior-project files")
require('web_server.on("/api/connect/start",HTTP_POST,captureStartRoute)' in main and
        'web_server.on("/api/connect/export",HTTP_GET,exportRoute)' in main,
        "protected Connect capture routes are missing")
require("web_server.onNotFound" in main and 'web_server.send(404,"application/json"' in main,
        "unknown or legacy web API routes do not fail closed")
require('web_server.authenticate("admin"' in main and "validCsrf(csrf_token" in main and
        "sameOrigin(origin.c_str()" in main,
        "Connect web authentication, CSRF or origin validation is missing")
for removed_route in ('/api/update', '/api/sharing', '/api/archive', '/api/forecast'):
    require(removed_route not in main, f"legacy weather API route remains: {removed_route}")
for legacy_tab in ('data-tab="forecast"', 'data-tab="alerts"', 'data-tab="weather"'):
    require(legacy_tab not in setup_page, f"legacy weather-service tab remains: {legacy_tab}")
for legacy_service in ('APRS', 'CWOP', 'Weather Underground', 'Weathercloud', 'NWS Alerts'):
    require(legacy_service not in setup_page,
            f"weather-service control or status remains in Connect page: {legacy_service}")
serial_status = main.partition('void status(')[2].partition('bool writeCaptureRecord')[0]
for legacy_status in ('forecast_periods', 'hourly_periods', 'sunrise', 'sunset',
                      'moon_phase', 'alerts_valid', 'alert_poll_ms', 'aprs_connected'):
    require(f'out["{legacy_status}"]' not in serial_status,
            f"legacy weather-service status remains in the Connect USB API: {legacy_status}")

# Exercise the exact allowlist implied by loadConnectSettings() against a
# synthetic pre-Connect settings record with every external service enabled.
legacy = {
    "wu_enabled": True, "weathercloud_enabled": True, "aprs_enabled": True,
    "update_enabled": True, "update_auto": True,
    "wu_key": "synthetic-secret", "weathercloud_key": "synthetic-secret",
    "aprs_passcode": "12345",
}
loaded_keys = set(re.findall(r'document\["([^"\\]+)"\]', services))
require(not (legacy.keys() & loaded_keys),
        f"old service settings are consumed: {legacy.keys() & loaded_keys}")
require(all(legacy.values()), "synthetic legacy config did not enable every service")

elf = FW / ".pio/build/panellan_sc05_x/firmware.elf"
require(elf.exists(), "firmware ELF is missing; build Connect before this audit")
strings = subprocess.run(["strings", str(elf)], check=True, capture_output=True, text=True).stdout
for forbidden in ("weatherstation/updateweatherstation.php", "api.weathercloud.net",
                  "rotate.aprs2.net", "sideout909/CYD-Weather-Link/releases/latest/download",
                  "CYDWeatherLink/"):
    require(forbidden not in strings, f"forbidden production client string is in firmware: {forbidden}")
require("https://github.com/sideout909/WX-Link/releases/latest/download/" in strings,
        "compiled firmware lacks only the fixed main-project migration manifest")

print("PASS: Connect namespace/path isolation, legacy settings allowlist, and compiled exporter/OTA strings")
