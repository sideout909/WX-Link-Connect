# Connect to main WX-Link transition

## Purpose and direction

WXLink Connect is installed first on volunteer devices that may receive stations not yet supported by the main WX-Link firmware. After the owner adds support and publishes a compatible main-project release, the volunteer can export evidence and deliberately replace Connect with that main release. The direction is one-way: Connect → `sideout909/WX-Link`. Connect does not install other repositories or transition back.

The published Connect repository prerelease is `0.1.1`; the current local-only test build is `0.1.4-dev`. The transition does not use Connect's own prerelease assets. It reads the main project's stable latest-release manifest at:

```text
https://github.com/sideout909/WX-Link/releases/latest/download/weather-link-panellan_sc05_x.json
```

As checked on 2026-10-22, the main repository's latest stable release was `v0.8.0`, with the expected SC05_X `.json` and `.bin` assets. The URL is intentionally dynamic so a later compatible stable release can be installed without updating Connect. GitHub's `latest` endpoint does not select prereleases.

The local 0.1.4 audit adds bounded, ticketed DNS and body reads, transport-lifetime
fixes and actionable retry errors. Its public main manifest/image were checked on
the host; ESP32 TLS/OTA acceptance is still pending. See [the audit](DEEP_AUDIT_0.1.4.md).

## User action and preconditions

The action is in the protected Connect Setup page as **Check and switch to WX-Link**. The browser first asks the user to confirm that wanted sessions have been exported and the station is supported, then requires the exact text `SWITCH TO WX-LINK`. The API requires the existing `admin` Basic Authentication, same-origin and CSRF checks. It returns an error if capture, storage, a capture lease or a raw-pulse window is active, or if Home Wi-Fi or valid system time is unavailable. The web routes contain no generic weather-service, sharing or production-updater actions; unknown paths return 404.

The worker is started during boot but sleeps until this explicit request. It has a fixed 16 KiB stack, performs no background release checks, and reports status through `/api/connect/status`. No work occurs in a decoder callback.

## Release validation and installation

Before writing the inactive app slot, Connect requires HTTPS with the bundled certificate roots and a valid clock. It limits the manifest to 4 KiB, accepts only the fixed repository's release asset URL, restricts redirect hosts to GitHub's asset hosts, and checks manifest schema, board (`panellan_sc05_x`), partition layout (`default_8MB-v1`), flash size, supported numeric version and inactive-slot size. The target must have a stable numeric version. Connect and main WX-Link are independent products, so their version numbers are not compared. The binary's ESP32-S3 application header and descriptor are checked; the received byte count and SHA-256 must match the manifest. A failed check aborts the update and leaves Connect running. On success, Connect schedules a restart into the main application; that boot path remains hardware-unverified.

The update writes the alternate application slot. Connect does not erase NVS or format LittleFS, and does not copy Wi-Fi, admin or station settings. It does not send captures or credentials over the network. Export wanted sessions before switching: the main firmware does not provide a Connect-session browser. Captures are stored under `/connect`; their retention across an actual device transition has not yet been verified on hardware. The main setup may require joining its setup network and entering its settings again.

The manifest hash checks image integrity against the manifest fetched over HTTPS. There is no separate publisher signature; trust depends on the GitHub release account, TLS certificate validation and the matching manifest. Do not describe this as signed firmware.

## Verification and remaining limits

Host checks cover fixed repository and asset paths, version ordering, board/partition/flash mismatch, slot size, malformed hashes, redirect-host rejection and compiled exclusion of the legacy exporter and generic updater. The project build and synthetic browser test cover compilation and the confirmation path. They do not execute a real HTTPS transition or establish hardware rollback, power-loss recovery, NVS/filesystem retention, station operation, RF performance or user-interface behavior on the display. Those checks remain part of M12 device acceptance.

No main WX-Link source or release was modified for this feature. The main release's own boot-confirmation and rollback behavior remains controlled by that firmware.
