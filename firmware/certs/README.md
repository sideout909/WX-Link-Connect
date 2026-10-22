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


## Connect update transport (0.1.5-dev)

Connect OTA now uses `github-roots.pem`, mirrored exactly in
`include/connect_tls_roots.h`. The roots were extracted from the host's trusted
public root store, matched by SHA256, not copied from a server's leaf or supplied
by a participant. Trusted anchors:

- Sectigo Public Server Authentication Root E46:
  `c90f26f0fb1b4018b22227519b5ca2b53e2ca5b3be5cf18efe1bef47380c5383`.
- USERTrust ECC Certification Authority:
  `4ff460d54b9c86dabfbcfc5712e0400d2bed3fbc4d4fbdaa86e06adcd2a9ad7a`.
- ISRG Root X1:
  `96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6`.

CA references: [Sectigo roots](https://www.sectigo.com/knowledge-base/detail/Sectigo-Root-Certificates/kA0Uj00000001s1),
[ISRG certificates](https://letsencrypt.org/certificates/).

The pinned Arduino 2.0.17 bundle verifier uses a dummy trust chain and returns
-12288 for multiple verification failures. Connect passes full public CA roots
to the normal mbedTLS verifier, including the explicit resolved-IP/hostname
connect overload. This retains certificate validity, signature and hostname
checks; it does not add an insecure fallback. The old compact bundle is retained
as a baseline asset but is not used by Connect's update transport.

Public server-chain fixtures were observed via direct OpenSSL TLS requests on
2026-10-22 to github.com and release-assets.githubusercontent.com, with SNI.
Both supplied cross-signed chains. Offline tests verify them at observation
time, reject a wrong hostname and verify PEM/header fingerprints. They do not
simulate the ESP32 allocator or establish success of a future CA rotation.
Re-review trust when GitHub changes its CA chain; no participant credentials
or private keys are included.
