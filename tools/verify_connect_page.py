#!/usr/bin/env python3
"""Run the embedded Connect page against synthetic local-only API replies."""
import json
from pathlib import Path
import shutil
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[1]
SESSION = "0123456789abcdef0123456789abcdef"


def main():
    from playwright.sync_api import sync_playwright

    page_html = (ROOT / "firmware/include/setup_page.h").read_text(encoding="utf-8")
    import re
    html = re.search(r'R"WXHTML\((.*)\)WXHTML";', page_html, re.S)[1]
    identity = '<img src=x onerror=alert(1)>'
    device = {"protocol": identity, "model": "Unknown", "id": "<b>sensor</b>",
              "channel": None, "packets": 4, "selected": False}
    session = {"session_id": SESSION, "decoded_records": 2, "pulse_records": 0,
               "decoded_valid_bytes": 300, "pulse_valid_bytes": 0, "interrupted": False}
    config = {"csrf": "a" * 32, "ssid": "Test Wi-Fi", "wifi_password_set": True,
              "timezone": "UTC0", "display_theme": "midnight", "date_format": "md",
              "clock_24h": False, "display_sleep_mode": "never", "display_timeout_s": 300}
    status = {"csrf": "a" * 32, "wifi": False, "active": False, "session_id": None,
              "capture_owner": "none", "capture_stop_reason": "none",
              "main_transition": {"worker_ready": True, "busy": False,
                                  "restart_pending": False, "progress": 0,
                                  "message": "Switch only after WX-Link includes support for this station."},
              "session_age_ms": None, "profile_id": "ook433-baseline-v1", "packet_records": 0,
              "pulse_records": 0, "drops": 0, "radio_ready": True, "storage_ready": True,
              "storage_free_bytes": 400000, "stored_sessions": 1,
              "preview": {"selected": True, "adapter_available": False,
                          "protocol": identity, "model": "Unknown", "id": "<b>sensor</b>",
                          "channel": None, "packets": 4, "last_age_ms": 10,
                          "temperature_c": 0, "wind_mph": None, "wind_direction_deg": None,
                          "humidity_pct": 0, "rain_counter": 0, "rain_unit": "in",
                          "light_lux": 0, "uv_index": None}}
    scan_mode = {"value": "normal", "polls": 0}
    config_failure = {"value": False}
    capture_failure = {"value": False}
    calls = []
    errors = []
    line = json.dumps({"schema": 1, "event": "session", "session_id": SESSION, "seq": 0,
                       "uptime_ms": 0, "utc_ms": None, "time_valid": False, "source": "live",
                       "profile_id": "ook433-baseline-v1"}) + "\n"

    def route(route, request):
        url = urlsplit(request.url)
        path = url.path
        if path == "/":
            route.fulfill(status=200, content_type="text/html", body=html)
        elif path == "/api/config" and request.method == "GET":
            route.fulfill(status=200, content_type="application/json", body=json.dumps(config))
        elif path == "/api/config" and request.method == "POST":
            assert request.headers["x-csrf-token"] == "a" * 32
            calls.append((path, request.post_data_json))
            if config_failure["value"]:
                route.fulfill(status=503, content_type="application/json", body='{"error":"settings_storage_failed"}')
            else:
                route.fulfill(status=200, content_type="application/json", body='{"ok":true,"restart_pending":true,"restart_delay_ms":2500}')
        elif path == "/api/connect/wifi-scan":
            if request.method == "POST":
                assert request.headers["x-csrf-token"] == "a" * 32
                scan_mode["polls"] = 0
                route.fulfill(status=200, content_type="application/json", body='{"state":"running","networks":[]}')
            else:
                scan_mode["polls"] += 1
                state = "running" if scan_mode["polls"] == 1 else ("failed" if scan_mode["value"] == "failed" else "complete")
                networks = [] if scan_mode["value"] == "empty" else [
                    {"ssid": identity, "rssi": -35, "secured": True},
                    {"ssid": "Guest open", "rssi": -60, "secured": False}]
                route.fulfill(status=200, content_type="application/json", body=json.dumps({"state": state, "networks": networks}))
        elif path == "/api/connect/packets":
            route.fulfill(status=200, content_type="application/json", body=json.dumps({
                "source": "live", "received_total": 12, "rejected": 1, "available": True,
                "native_json": '{"model":"' + identity + '","unknown":1.2300}'}))
        elif path == "/api/connect/status":
            route.fulfill(status=200, content_type="application/json", body=json.dumps(status))
        elif path == "/api/connect/devices":
            route.fulfill(status=200, content_type="application/json", body=json.dumps({"csrf": "a" * 32, "devices": [device]}))
        elif path == "/api/connect/sessions":
            route.fulfill(status=200, content_type="application/json", body=json.dumps({"csrf": "a" * 32, "sessions": [session]}))
        elif path == "/api/connect/export":
            query = parse_qs(url.query)
            assert query["revision"] == ["300"]
            route.fulfill(status=200, content_type="application/x-ndjson",
                            headers={"X-Next-Offset": "300", "X-Stream-Revision": "300", "X-Session-Done": "true"}, body=line)
        elif path in ("/api/connect/preview", "/api/connect/start", "/api/connect/stop",
                      "/api/connect/heartbeat",
                      "/api/connect/raw-window", "/api/connect/delete",
                      "/api/connect/switch-to-wx-link"):
            assert request.headers["x-csrf-token"] == "a" * 32
            calls.append((path, request.post_data_json))
            if path.endswith("switch-to-wx-link"):
                assert request.post_data_json == {"confirmation": "SWITCH TO WX-LINK"}
                status["main_transition"].update(busy=True, progress=0,
                                                  message="WX-Link transition queued")
            if path.endswith("/start"):
                if capture_failure["value"]:
                    route.fulfill(status=409, content_type="application/json", body=json.dumps({"error":"Capture files could not be opened; check storage status."}))
                    return
                status["active"] = True
                status["capture_owner"] = "device"
            elif path.endswith("/stop"):
                status["active"] = False
                status["capture_owner"] = "none"
            route.fulfill(status=200, content_type="application/json",
                            body=json.dumps({"ok": True, "session_id": SESSION, "capture_owner": status["capture_owner"]}))
        else:
            errors.append(path)
            route.fulfill(status=404, body="not found")

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=shutil.which("brave") or "/usr/bin/brave",
                                             headless=True, args=["--no-sandbox"])
        page = browser.new_page(accept_downloads=True)
        page.set_default_timeout(5000)
        page.on("pageerror", lambda error: errors.append(str(error)))
        def accept_expected_dialog(dialog):
            if dialog.type == "prompt":
                dialog.accept("SWITCH TO WX-LINK")
            else:
                dialog.accept()
        page.on("dialog", accept_expected_dialog)
        page.route("**/*", route)
        page.goto("http://wxconnect.test/", wait_until="domcontentloaded")
        page.locator("#identity").wait_for()
        assert page.locator("#identity").inner_text() == identity + " · Unknown · ID <b>sensor</b>"
        assert page.locator("img").count() == 0
        assert page.locator("#temperature").inner_text() == "32.0 °F"
        assert page.locator("#humidity").inner_text() == "0%"
        assert page.locator("#rain").inner_text() == "0.00 in"
        page.get_by_role("button", name="Devices").click()
        page.get_by_role("button", name="Preview").click()
        page.get_by_role("button", name="Capture").click()
        # Storage and retained-session blockers must be actionable, not a silent disabled button.
        status.update(storage_ready=False, capture_start_blocker="Capture storage is unavailable; check storage status. No data was erased.", storage_message="Capture directory unavailable")
        page.evaluate("refresh()")
        assert page.locator("#start").is_disabled()
        assert "unavailable" in page.locator("#capture-readiness").inner_text()
        assert page.locator("#storage-status").inner_text() == "Capture directory unavailable"
        status.update(storage_ready=True, capture_start_blocker="Eight sessions retained. Export and delete a Connect session to start another.")
        page.evaluate("refresh()")
        assert "Export and delete" in page.locator("#capture-readiness").inner_text()
        status.update(capture_start_blocker=None)
        page.evaluate("refresh()")
        capture_failure["value"] = True
        page.locator("#start").click()
        page.wait_for_function("document.querySelector('#capture-feedback').textContent.includes('could not be opened')")
        capture_failure["value"] = False
        page.locator("#start").click()
        page.wait_for_function("document.querySelector('#capture-state').textContent.includes('continues with this page closed')")
        assert page.locator("#start").inner_text() == "Recording is running"
        page.locator("#raw-enable").check()
        page.locator("#raw-start").click()
        page.locator("#stop").click()
        # Legacy browser lease handling is still supported without assigning it to new starts.
        status.update(active=True,capture_owner="browser",session_id=SESSION)
        page.evaluate("refresh()")
        page.evaluate("heartbeat()")
        status.update(active=False,capture_owner="none",session_id=None)
        page.evaluate("refresh()")
        with page.expect_download():
            page.get_by_role("button", name="Download decoded").click()
        page.get_by_role("button", name="Delete").click()
        assert "1.2300" in page.locator("#live-packet").inner_text()
        assert identity in page.locator("#live-packet").inner_text()
        assert page.locator("img").count() == 0
        status.update(active=True, capture_owner="device")
        page.evaluate("refresh()")
        assert page.locator("#stop").is_enabled()
        assert page.locator("#start").is_disabled()
        page.locator("#stop").click()
        page.get_by_role("button", name="Setup").click()
        page.wait_for_function("document.querySelector('#wifi-networks').options.length===3")
        assert page.locator('[name="ssid"]').input_value() == "Test Wi-Fi"
        assert identity in page.locator("#wifi-networks").inner_text()
        assert page.locator("img").count() == 0
        page.locator("#wifi-networks").select_option("Guest open")
        assert page.locator('[name="clear_wifi_password"]').is_checked()
        page.locator("#wifi-networks").select_option(identity)
        assert not page.locator('[name="clear_wifi_password"]').is_checked()
        assert page.locator('[name="ssid"]').input_value() == identity
        page.locator('[name="ssid"]').fill("Hidden manual network")
        scan_mode["value"] = "empty"
        page.locator("#wifi-scan").click()
        page.wait_for_function("document.querySelector('#wifi-scan-feedback').textContent.includes('No visible')")
        assert page.locator('[name="ssid"]').input_value() == "Hidden manual network"
        scan_mode["value"] = "failed"
        page.locator("#wifi-scan").click()
        page.wait_for_function("document.querySelector('#wifi-scan-feedback').textContent.includes('failed')")
        assert page.locator("#wifi-scan").is_enabled()
        config_failure["value"] = True
        page.locator('[name="wifi_password"]').fill("synthetic-password")
        page.locator("#save-setup").click()
        page.wait_for_function("document.querySelector('#setup-feedback').textContent.includes('settings_storage_failed')")
        assert page.locator('[name="wifi_password"]').input_value() == "synthetic-password"
        assert page.locator("#save-setup").is_enabled()
        config_failure["value"] = False
        page.locator("#save-setup").click()
        page.wait_for_function("document.querySelector('#setup-feedback').textContent.includes('Restarting')")
        assert page.locator("#save-setup").is_disabled()
        assert page.locator('[name="wifi_password"]').input_value() == ""
        scan_mode["value"] = "normal"
        page.reload(wait_until="domcontentloaded")
        page.get_by_role("button", name="Setup").click()
        page.wait_for_function("document.querySelector('#wifi-networks').options.length===3")
        page.locator("#switch-main").click()
        page.wait_for_function("document.querySelector('#switch-state').textContent.includes('queued')")
        assert not errors, errors
        assert any(path == "/api/connect/raw-window" and data["duration_ms"] == 1000
                   for path, data in calls if data)
        assert any(path == "/api/connect/heartbeat" and data["session_id"] == SESSION
                   for path, data in calls if data)
        assert any(path == "/api/connect/delete" for path, _ in calls)
        assert any(path == "/api/config" for path, _ in calls)
        assert any(path == "/api/connect/switch-to-wx-link" and
                   data == {"confirmation": "SWITCH TO WX-LINK"} for path, data in calls)
        print("PASS: Wi-Fi scan, hostile SSIDs, hidden/open networks, save failure/restart feedback, automatic-capture Stop and live packet view; Connect page renders untrusted RF identity as text and exercises controls, download, delete, setup and confirmed main transition against synthetic APIs")
        browser.close()


if __name__ == "__main__":
    main()
