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
            route.fulfill(status=200, content_type="application/json", body='{"ok":true}')
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
                status["active"] = True
                status["capture_owner"] = "browser"
            elif path.endswith("/stop"):
                status["active"] = False
                status["capture_owner"] = "none"
            route.fulfill(status=200, content_type="application/json",
                            body=json.dumps({"ok": True, "session_id": SESSION}))
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
        page.locator("#start").click()
        page.locator("#raw-enable").check()
        page.locator("#raw-start").click()
        page.locator("#stop").click()
        with page.expect_download():
            page.get_by_role("button", name="Download decoded").click()
        page.get_by_role("button", name="Delete").click()
        page.get_by_role("button", name="Setup").click()
        page.locator("#switch-main").click()
        page.wait_for_function("document.querySelector('#switch-state').textContent.includes('queued')")
        page.locator("#settings").evaluate("form => form.requestSubmit()")
        page.wait_for_timeout(100)
        assert not errors, errors
        assert any(path == "/api/connect/raw-window" and data["duration_ms"] == 1000
                   for path, data in calls if data)
        assert any(path == "/api/connect/heartbeat" and data["session_id"] == SESSION
                   for path, data in calls if data)
        assert any(path == "/api/connect/delete" for path, _ in calls)
        assert any(path == "/api/config" for path, _ in calls)
        assert any(path == "/api/connect/switch-to-wx-link" and
                   data == {"confirmation": "SWITCH TO WX-LINK"} for path, data in calls)
        print("PASS: Connect page renders untrusted RF identity as text and exercises controls, download, delete, setup and confirmed main transition against synthetic APIs")
        browser.close()


if __name__ == "__main__":
    main()
