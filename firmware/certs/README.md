# HTTPS trust bundle

`x509_crt_bundle.bin` contains public CA roots, generated with Espressif's
[ESP-IDF v4.4.7 bundle generator](https://github.com/espressif/esp-idf/blob/v4.4.7/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py)
from the development PC's `/etc/ca-certificates/extracted/tls-ca-bundle.pem`
on October 20, 2026 (121 certificates). The PlatformIO build embeds this bundle.
NWS and PWS HTTPS clients verify server certificates and hostnames; a valid
network-synchronized clock is required. No `setInsecure()` fallback is used.

Regenerate periodically from an updated, trusted public root store. This file
contains no private keys or station credentials.

The sun/moon client uses the public [GTS Root R4 PEM](https://pki.goog/repo/certs/gtsr4.pem)
embedded in `include/sun_root.h`. Its SHA-256 fingerprint is
`349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d`,
verified against the PC's trusted CA store. This allows mbedTLS to anchor the
Google chain directly at R4; the older IDF bundle verifier failed on this
service's cross-signed chain with `MBEDTLS_ERR_X509_FATAL_ERROR` (-12288).
It is a CA trust anchor, not a leaf-certificate pin. Certificate/hostname
verification remains enabled. Update trust if the provider changes CA.
