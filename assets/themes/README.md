# Shared display and webpage themes

`palettes.json` is the runtime source for Midnight (original), Matrix, Amber,
Ice Blue, Violet, Paper and Night (low light). IDs are persisted as `display_theme`; keep them
stable. Missing settings default to Midnight. Invalid IDs/types cannot be saved.

From the repository root:

```bash
python tools/pack-display-themes.py
python tools/pack-setup-assets.py
```

The first command generates portable RGB565 values in
`firmware/include/display_themes.h`, CSS in the embedded setup page and original
SVG-derived PNG arrays in `firmware/include/theme_icons.h`. It needs Python 3
and `rsvg-convert`. The second packs browser scripts as deterministic gzip.
Normal firmware builds need neither graphics tools nor runtime SVG rendering.

The webpage previews a selected palette locally. Save and restart persists the
selection for both display and browser. Original icon shapes and Home geometry
are preserved; Matrix also makes the sun green. The phase-aware moon remains separate; Night tints it amber. Night also uses
24/255 normal backlight and 2/255 dimmed, compared with 160/16 for other themes.
Its NWS all-clear/connected green is softened to olive, with near-black surfaces. Maps retain their provider's cartography.

Display watch/warning themes override the normal palette. Browser alert cards
retain explicit contrasting alert colors; normal palette choice cannot hide
warning semantics. Paper uses darker orange for stale readings. Host tests
check text/secondary-text contrast on background/cards and visible accents;
browser tests cover all themes, persistence submission/reload and Paper alerts.

The original [concepts](../../concepts/themes/README.md) remain design artifacts,
including the speaker icon that was removed from the actual device in v0.7.0.
