"""Offline public-chain tests; these do not execute ESP32 mbedTLS."""
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import re
import ssl
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
ROOTS = ROOT / "firmware/certs/github-roots.pem"
FIXTURES = ROOT / "firmware/tests/fixtures/github-tls"


class ConnectTrustTests(unittest.TestCase):
    def test_public_roots_match_embedded_copy(self):
        pem = ROOTS.read_text()
        embedded = (ROOT / "firmware/include/connect_tls_roots.h").read_text()
        self.assertIn('R"WXCA(' + pem + ')WXCA"', embedded)
        certs = re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", pem, re.S)
        self.assertEqual({hashlib.sha256(ssl.PEM_cert_to_DER_cert(c)).hexdigest() for c in certs}, {
            "c90f26f0fb1b4018b22227519b5ca2b53e2ca5b3be5cf18efe1bef47380c5383",
            "4ff460d54b9c86dabfbcfc5712e0400d2bed3fbc4d4fbdaa86e06adcd2a9ad7a",
            "96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6",
        })

    def test_observed_cross_signed_chains_and_hostname_checks(self):
        # Verify at observation time so an intentionally preserved leaf fixture
        # expiring later does not turn an offline regression into a network task.
        timestamp = str(int(datetime(2026, 10, 22, 18, tzinfo=timezone.utc).timestamp()))
        for host in ("github.com", "release-assets.githubusercontent.com"):
            with self.subTest(host=host), tempfile.TemporaryDirectory() as directory:
                chain = Path(directory) / "intermediates.pem"
                chain.write_text("".join((FIXTURES / f"{host}.{i}.pem").read_text() for i in (1, 2)))
                command = ["openssl", "verify", "-no-CApath", "-no-CAstore", "-CAfile", str(ROOTS),
                           "-attime", timestamp, "-untrusted", str(chain), "-verify_hostname"]
                leaf = str(FIXTURES / f"{host}.0.pem")
                good = subprocess.run(command + [host, leaf], capture_output=True, text=True)
                self.assertEqual(good.returncode, 0, good.stderr)
                bad = subprocess.run(command + ["wrong.example.invalid", leaf], capture_output=True, text=True)
                self.assertNotEqual(bad.returncode, 0)


if __name__ == "__main__":
    unittest.main()
