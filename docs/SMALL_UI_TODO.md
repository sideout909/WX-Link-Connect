# Small UI handoff

Both cosmetic changes are implemented locally in the 0.1.5-dev candidate,
following the owner's request to apply them.

- The Home decoded-packet footer uses a plain ASCII hyphen instead of the
  separator glyph that appeared as an unexpected symbol in the embedded font.
- The top-right "WiFi setup" prompt uses fixed orange (RGB 255, 165, 0).
  The connected "WiFi +" indicator remains green.
- The browser header badge is orange during setup too.
- Home identity/status separators now use ASCII; temperature and wind degree
  marks are drawn as circles rather than unsupported Font0 extended glyphs.

Verification: `bash tools/pio.sh run -d firmware` passed for the preserved
PanelLan SC05_X profile; `git diff --check` passed. Build log:
`artifacts/deep-audit/cosmetic-build.log`.

The owner confirmed the corrected appearance. Before/after screenshots were
read from the physical device and reviewed locally; the 0.1.5 screenshot shows
correct degree marks and separators. Browser computed-color checks verify orange
before setup and its removal after Wi-Fi connects. See UPDATE_AND_DISPLAY_0.1.5.md.
No push or release was performed.
