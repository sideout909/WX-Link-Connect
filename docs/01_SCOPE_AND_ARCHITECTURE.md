# Scope and architecture

## Explicit interpretation of the owner's request

WXLink Connect is a dedicated community capture firmware derived from the existing
PanelLan weather project. Volunteers use the same PanelLan ZX2D80CE02S-V10/SC05_X,
ESP32-S3 N8R2 and receive-only 433 MHz CC1101. A browser downloads captures; a PC
USB logger is an alternative. It is not a desktop-only program, hosted website,
cloud database, new production weather decoder or generic SDR application. If the
owner changes this interpretation, update this scope before coding. The owner has
authorized one deliberate, one-way OTA transition from Connect to the main
`sideout909/WX-Link` application after station support is published there.

## Definition of a completed first release

A new volunteer can follow wiring/setup instructions, see real sensor data when
a reviewed preview adapter recognizes it, start/stop a bounded recording, see
packet/pulse/drop counts and download a self-contained bundle for manual sharing.
Another developer can distinguish repeated packets, missing samples, receiver
profiles, invalid UTC and restarts, and use the original JSON/pulses to investigate
station support without guessing how data was normalized. The firmware never
publishes weather observations to a third party.

First support the known Atlas OOK receiver as the reference case. Then deliberately
enable a reviewed 433 MHz OOK decoder subset. Iris/Notos, Cotech and La Crosse are
research candidates, not hardware compatibility claims. FSK is a separate profile
milestone: one CC1101 cannot listen to OOK and FSK simultaneously. Never assume
all models from a brand use the same band/protocol. A decoder's fields[] list is
not its guaranteed callback schema. See [the Connect decoder profile](DECODER_PROFILE.md)
and the [reviewed decoder inventory](../reference/decoder-inventory/433-weather.csv).

## Data path and ownership

CC1101 capture -> vendored decoder -> bounded callback queues -> main-loop broker.
The broker assigns session/record sequencing and sends data independently to:
- Capture storage/USB transport: original native JSON and optional pulse records.
- Candidate table: per-protocol/model/ID/channel identity, counts and last receive.
- Preview adapters: selected identity only, known unit conversions, per-field ages.
- Display/browser: read-only snapshots and capture controls.

Do not put weather::Store/atlas::decode in front of the recorder. Current liveReceive
filters through Atlas and station_confirmed before rf_packet export. It therefore
cannot be reused unchanged to collect unknown/foreign stations. Capture before
that path; ultimately remove the irrelevant production weather ingestion/export path.

The decoder task may only copy bounded data. The main loop owns application state;
network handlers do not mutate decoder structures. The file writer has explicit
ownership/queueing, with short bounded writes. Never copy callback pointers for
later use; JSON and pulse buffers are borrowed memory. A missing/full queue increments
a drop counter; do not block RF waiting for storage or browser downloads.

## Hardware that must stay unchanged

CC1101: SCK GPIO10, MISO11, MOSI12, CS13, GDO0 GPIO14, GDO2 GPIO21. Power uses
debug pin 2 +3.3 V and pin 7 GND; six signals use the expansion header. Follow
the [wiring guide](WIRING.md) for exact orientation. Expansion pin 1 is 5 V: not
radio VCC. Debug-supply spare capacity has not been measured on the Connect
configuration. No SD, second processor or extra radio is required. Do not copy a generic
ILI9341 CYD profile: this board has parallel ST7789 and FT5x06 touch.

## Radio source facts already checked

The copied library has setRawPulsesCallback. Signature:
const int* pulse_us, const int* gap_us, unsigned num_pulses,
unsigned long duration_us, int rssi. Pointers live only until callback return.
In signalDecoder.cpp it is invoked after decoder processing, including zero-decode
signals that reached that stage. It is not raw IQ, is not every RF event and may
already have passed RSSI/minimum-pulse capture gates. JSON callbacks may precede
pulse callbacks: do not invent a correlation ID based solely on ordering.

CC1101 OOK requests CC1101_RX_BANDWIDTH=812.0f, mapped by the driver to 812.5 kHz
with the documented 26 MHz crystal. The nearby 270 kHz branch is not the current
CC1101 OOK path. Keep AGC defaults C7/40/B2 and restart capture margin 9 dB until
a controlled profile experiment records the change. RSSI floor/threshold is not
packet RSSI. Record requested/applied settings separately; unknown readback is null.

## Explicit exclusions

No automatic community upload, backend, accounts, APRS/CWOP/WU/Weathercloud, NWS,
generic or automatic OTA, weather-station binding, full-day rain claim, all-band
sweep, arbitrary register UI, transmit/replay-to-air or SD requirement. The only
Connect OTA path is the protected, typed-confirmation transition to the fixed main
WX-Link release repository; it checks the SC05_X board/partition, newer version,
image header and manifest SHA-256, and writes only the inactive application slot.
It does not copy Connect credentials/settings, erase capture files, or let users
choose a repository. Volunteers must export captures first because WX-Link does
not browse Connect sessions. No routine update checks or external weather uploads
run from Connect. No sound. Keep useful original fonts/icons/themes, but do not
build new themes or animations. Missing weather data must not be filled with
forecasts or invented values.
