import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("package_connect", Path(__file__).with_name("package-connect-update.py"))
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class ConnectUpdatePackageTests(unittest.TestCase):
    def test_manifest_and_image_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "synthetic-test.bin"
            data = bytearray(32768)
            data[0] = 0xE9
            data[12:14] = (9).to_bytes(2, "little")
            data[32:36] = (0xABCD5432).to_bytes(4, "little")
            identity = b"WXLink Connect 0.1.6\0sideout909/WX-Link-Connect\0"
            data[100:100 + len(identity)] = identity
            binary.write_bytes(data)
            manifest = PACKAGER.package(binary, root / "out", "0.1.6")
            saved = json.loads((root / "out/WXLink-Connect-v0.1.6-manifest.json").read_text())
            self.assertEqual(saved, manifest)
            self.assertEqual(manifest["repository"], PACKAGER.REPOSITORY)
            self.assertTrue(manifest["firmware_url"].endswith("/v0.1.6/WXLink-Connect-v0.1.6.bin"))
            for version in ("0.1.7", "0.1.6-dev", "../bad", "0.99999.0"):
                with self.assertRaises(ValueError):
                    PACKAGER.package(binary, root / "bad", version)
            data[12] = 0
            binary.write_bytes(data)
            with self.assertRaises(ValueError):
                PACKAGER.package(binary, root / "bad", "0.1.6")


if __name__ == "__main__":
    unittest.main()
