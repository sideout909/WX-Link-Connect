from html.parser import HTMLParser
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
PAGE = ROOT / "firmware/include/setup_page.h"


class IDs(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.tabs = set()

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        if tag == "button" and "data-tab" in attrs:
            self.tabs.add(attrs["data-tab"])


class ConnectPageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = PAGE.read_text(encoding="utf-8")
        cls.html = re.search(r'R"WXHTML\((.*)\)WXHTML";', cls.header, re.S).group(1)

    def test_capture_product_has_only_connect_tabs_and_routes(self):
        parser = IDs()
        parser.feed(self.html)
        self.assertEqual(parser.tabs, {"overview", "capture", "devices", "setup"})
        self.assertIn("/api/connect/start", self.html)
        self.assertIn("/api/connect/heartbeat", self.html)
        self.assertIn("/api/connect/export?", self.html)
        self.assertIn("/api/connect/switch-to-wx-link", self.html)
        self.assertIn("/api/connect/update", self.html)
        self.assertIn("UPDATE CONNECT", self.html)
        for removed in ("/api/update", "/api/sharing", "/api/forecast", "/api/archive",
                        "Weather Underground", "APRS / CWOP"):
            self.assertNotIn(removed, self.html)

    def test_rf_identity_is_rendered_as_text(self):
        self.assertIn("$('identity').textContent=", self.html)
        self.assertIn("label.textContent=", self.html)
        self.assertNotRegex(self.html, r"\binnerHTML\b|insertAdjacentHTML|\beval\s*\(")

    def test_capture_controls_keep_preview_and_recording_independent(self):
        self.assertIn("Preview selection only changes Home", self.html)
        self.assertIn("Recording is independent", self.html)
        self.assertIn("id=\"raw-enable\"", self.html)
        self.assertIn("id=\"raw-seconds\"", self.html)
        self.assertIn("maximum 8 retained", self.html)

    def test_browser_capture_renews_only_through_owned_heartbeat(self):
        self.assertIn("setInterval(heartbeat,5000)", self.html)
        self.assertIn("s.capture_owner==='browser'", self.html)

    def test_main_transition_is_fixed_target_and_explicit(self):
        self.assertIn("sideout909/WX-Link", self.html)
        self.assertIn("Type SWITCH TO WX-LINK", self.html)
        self.assertIn("Connect Wi-Fi/admin settings are not copied", self.html)


if __name__ == "__main__":
    unittest.main()
