# WXLink Connect SC05_X and CC1101 wiring

This wiring applies only to PanelLan ZX2D80CE02S-V10 / SC05_X, board profile
`panellan_sc05_x`, and the pictured-style eight-pin CC1101 module connector.
Identify connector numbering from the board markings/manufacturer drawing; do
not infer its orientation by counting from the radio module photograph.

| CC1101 module pin | Signal | PanelLan connector | Board pin |
| --- | --- | --- | --- |
| 1 | GND | 7-pin debug header | pin 7 · GND |
| 2 | VCC | 7-pin debug header | pin 2 · +3.3 V |
| 3 | GDO0 | 8-pin expansion header | pin 7 · GPIO14 |
| 4 | CSN | 8-pin expansion header | pin 6 · GPIO13 |
| 5 | SCK | 8-pin expansion header | pin 3 · GPIO10 |
| 6 | MOSI | 8-pin expansion header | pin 5 · GPIO12 |
| 7 | MISO/GDO1 | 8-pin expansion header | pin 4 · GPIO11 |
| 8 | GDO2 | 8-pin expansion header | pin 8 · GPIO21 |

The six digital signals use the expansion header. Expansion pin 1 is +5 V and
must never connect directly to CC1101 VCC. The debug header provides +3.3 V on
pin 2, but its available output current is unspecified. Before powering a radio,
check voltage and wiring with power disconnected, then verify loaded voltage
near 3.3 V while display, Wi-Fi and receiver are active. Use a separate suitable
3.3 V regulator if the board supply droops; never back-power the ESP32.

Use short wires, a suitable 433 MHz antenna, and local decoupling appropriate to
the radio module. The module supply must remain within the CC1101's 1.8–3.6 V
range. The current firmware uses the SC05_X parallel ST7789 display and FT5x06
touch profile; generic CYD/ILI9341 pin maps do not apply. See the local
[hardware references](../reference/README.md) and board [scope](01_SCOPE_AND_ARCHITECTURE.md).

No physical wiring or power measurement was performed for this software task.
