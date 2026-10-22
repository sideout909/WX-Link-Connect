# Embedded browser assets

`dashboard.js` supplies the protected phone overview, bounded minute-history
reads and cached NWS cards. Clock/date preferences and condition artwork match
the panel; see [dashboard behavior](../../firmware/WEB_DASHBOARD.md).

`discovery.js` supplies the local automatic-station setup flow in v0.5.1. It
contacts only authenticated device APIs, never an external discovery service.
Candidates require repeated native RF transmissions; Use this station stages
the selection for the existing Save settings action. Manual IDs stay under
Advanced. See [decoder/discovery behavior](../../DECODER_SUPPORT.md) and run
`tools/verify_discovery_setup.py` with the project's browser-test environment.

`location.js` runs in the phone/desktop browser. The ESP32 serves the page and
local scripts; it does not render maps, proxy tiles or perform location lookups.
Assets are packed with `python tools/pack-setup-assets.py` into
`firmware/include/setup_assets.h` using deterministic gzip. Normal builds need
no npm, CDN download or desktop map dependency.

## Sources and behavior

- [Leaflet 1.9.4](https://leafletjs.com/download.html), the stable browser
  library, is vendored unchanged under `leaflet/`. Its BSD license accompanies
  the sources. The packer verifies the JS/CSS SHA-256 values against the
  pinned files; they match Leaflet's published SRI hashes. Custom CSS pin
  graphics avoid requesting Leaflet's optional marker/layer images.
- [Zippopotam.us postal lookup](https://docs.zippopotam.us/docs/v1/) centers a
  US ZIP area. ZIP+4 uses its first five digits. Only explicit button/Enter
  requests run; there is no address autocomplete or background geocoding.
  [GeoNames](https://www.geonames.org/) supplies its postal data and is
  attributed alongside Zippopotam.us.
- [OpenStreetMap standard tiles](https://operations.osmfoundation.org/policies/tiles/)
  are fetched directly by the browser over HTTPS for the current interactive
  viewport, with attribution and an origin Referer. Browser caching follows
  server headers; no prefetch/offline archive is offered. Tests intercept all
  tile requests locally and never pan an automated browser against live OSM.
  Review service policy/capacity and choose an appropriate provider before
  a large public deployment; no service availability is guaranteed.
- [USGS EPQS](https://epqs.nationalmap.gov/v1/docs) returns meters for the
  confirmed candidate's longitude/latitude (`wkid=4326`). The browser displays
  meters and feet; saved elevation is meters. This is a [ground estimate from
  3DEP](https://www.usgs.gov/faqs/how-accurate-are-elevations-generated-elevation-point-query-service-national-map),
  not the height of the pole/sensor, and coverage is primarily US terrain.
  Missing/out-of-range data leaves manual elevation required rather than
  assigning zero or carrying the previous site's elevation.

Provider requests use `credentials: omit`. The page explains the ZIP, point
or map area goes to those providers when the user opens the tools. No device
admin password, Wi-Fi password, upload key or APRS passcode is sent to them.
No provider requests run simply because the setup page loads. Manual setup
continues to work without internet; map controls are served locally as well.

ZIP centering never changes saved form values. A dropped/dragged pin starts
a bounded elevation lookup; obsolete responses are ignored. Use this location
copies the point to the form and explicitly selects Location is configured.
Save and restart is still required to persist it. Sharing switches and station
identity are preserved; timezone remains an explicit user choice.

## Local dashboard

`dashboard.js` renders the protected Overview using device status, paginated
minute history, and cached NWS forecasts/alerts. It requests only same-origin
device endpoints and local weather icons; it sends no provider requests or
credentials elsewhere. Pack it with `tools/pack-setup-assets.py`. The display
contract and test commands are in `firmware/WEB_DASHBOARD.md`.


Shared palette source is `assets/themes/palettes.json`. Run
`python tools/pack-display-themes.py` to regenerate embedded theme CSS, firmware
RGB565 colors and original weather icon variants; then repack browser assets.
The dashboard previews theme changes locally and applies the saved theme after
configuration loads. Warning/watch cards retain independent contrasting colors.
