# Public GitHub TLS chain fixtures

These are public certificates obtained with `openssl s_client -showcerts` using
SNI for github.com and release-assets.githubusercontent.com on 2026-10-22.
Files numbered 0/1/2 are the leaf and the two server-supplied chain certificates.
They are offline test evidence, not trusted roots or firmware leaf pins.
The explicit trusted roots and fingerprints are documented in firmware/certs/README.md.
`tools/test_connect_tls.py` checks hostname/chain verification at observation time,
plus rejection of a wrong hostname, without internet or hardware access.
Certificates are public CA-issued metadata; no device capture, credentials or
private key is included. These tests use host OpenSSL, not ESP32 mbedTLS.
