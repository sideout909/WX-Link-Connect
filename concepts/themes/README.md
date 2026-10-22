# Local Home palette concepts

Five color-only studies: Matrix, Amber, Ice Blue, Violet and Paper. These are
the original local design artifacts. Their palettes are now implemented in
local v0.7.0 firmware, using assets/themes/palettes.json as the runtime source;
no GitHub release was published. The study SVGs retain their original sample
header, including the since-removed speaker icon.

- Open [the interactive preview](index.html) in a browser; select a theme to
  enlarge it or compare all five below. It works directly from disk, offline.
- [Comparison PNG](comparison.png) and [editable comparison SVG](comparison.svg)
  show the same sample layout/readings in every palette.
- Each theme has a separate native 320×240 SVG. [Palette values](palettes.json)
  record background, card, text, secondary text, accent, divider, connected,
  normal alert-card and border colors.

The shared geometry comes from the existing Home v2 SVG, with current labels,
identity/status row, UV, BARO and packet age added. These are vector concepts,
not framebuffer captures or claims of pixel-exact firmware output. Weather
readings and N0CALL-1 are fictional; no configuration or secrets are embedded.
Matrix also tints the sun green to keep the phosphor appearance. Other themes
retain the warm sun. Weather graphic shapes, text positions and dimensions
are identical across the five studies.

Existing orange watch/red warning overrides and stale-temperature indicators
remain separate from normal palette choice. Alert variants are not part of
this preview. No new navigation or firmware behavior is proposed here.

Regenerate from the repository root:

```bash
python concepts/themes/generate.py
```

Requires Python's standard library and `rsvg-convert` for the comparison PNG.
The interactive preview has no external requests, libraries or build step.
