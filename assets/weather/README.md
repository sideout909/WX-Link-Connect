# Weather graphics

Twelve original vector icons extend the home screen's rounded cloud, cyan outline, and warm sunshine style. Each has an editable 48×48 SVG and a transparent 48×48 RGBA PNG. View the [contact sheet](../../concepts/weather-icons.png).

| Files | Intended source |
| --- | --- |
| `sunny`, `partly-cloudy`, `cloudy` | Labeled, calibrated lux-based estimate during suitable daylight; alternatively a separately labeled external conditions source. |
| `rain`, `heavy-rain` | Fresh rain-counter changes/rate with documented windows and validated thresholds. |
| `thunderstorm` | Fresh supported local lightning evidence or an explicitly labeled external condition source. A watch alone cannot establish a storm at the sensor. |
| `night` | Valid time and coordinates establishing nighttime; no claim of clear skies. |
| `partly-cloudy-night`, `snow`, `fog` | Available to an explicitly attributed forecast source; lux/temperature alone are insufficient. |
| `windy` | Fresh measured wind with a documented threshold/window. |
| `unknown` | Missing, stale, uncalibrated, or unsupported conditions. |

Current firmware selects these assets from fresh NWS forecast text, not from
a calibrated lux/rain classifier. Home uses current moon phase with subtle
craters and cloud overlays; forecast periods use illustrative night moons, including behind fog and
precipitation artwork. Unknown conditions are not turned into clear nights.
NWS is labeled above the Home icon. The source table also records possible
future local-evidence uses.

Icon availability does not imply a working classifier or a new sensing capability. See [HOME_SCREEN.md](../../HOME_SCREEN.md) for condition selection, source labeling, and overlapping conditions. Integrate the PNGs or generate library-compatible bitmaps at the final display size; runtime SVG support is not assumed.

Regenerate icons and revised concept sheets from the repository root:

```sh
python concepts/generate_weather_assets.py
```

Requires Python 3 and `rsvg-convert` (librsvg). The generator uses only Python's standard library and preserves `concepts/home-screen.svg` as the original layout reference. It replaces generated icon files, contact sheets, and `home-screen-v2` previews; edit the generator for reproducible changes. It does not generate or implement firmware.


`tools/pack-display-themes.py` derives five colored variants of these original
SVGs into `firmware/include/theme_icons.h`. Geometry is unchanged; moon phase
rendering stays separate. The original Midnight PNGs remain the fallback set.
Browser icon requests can select one of the six fixed theme IDs; unknown
names/themes are rejected. See `assets/themes/palettes.json`.
