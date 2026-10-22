# Third-party notices

Original project code uses [GPL-3.0-only](../LICENSE). The components below
retain their own licenses and copyright notices.

Pinned source dependencies and reproducible build configuration are in
[platformio.ini](../firmware/platformio.ini). The dependency sources are fetched
by PlatformIO; they are not private device configuration. Retained license
texts for application libraries are in [licenses/](licenses/).

| Component | Pinned source/version | Notice |
| --- | --- | --- |
| rtl_433_ESP, including rtl_433 decoder code | [f0a8156](https://github.com/NorthernMan54/rtl_433_ESP/tree/f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8) | [Bundled GPL-3.0 license](licenses/rtl_433_ESP-GPL-3.0.txt); preserve individual source notices. |
| PanelLan | [2bc0d08](https://github.com/smartpanle/PanelLan_esp32_arduino/tree/2bc0d086c9a55f5288d85da6c16968c22744fe5f) | [MIT](licenses/PanelLan-MIT.txt) |
| RadioLib | [7.7.1](https://github.com/jgromes/RadioLib/tree/7.7.1) | [MIT](licenses/RadioLib-MIT.txt) |
| LovyanGFX | [1.2.7](https://github.com/lovyan03/LovyanGFX/tree/1.2.7) | [Bundled MIT/BSD and font notices](licenses/LovyanGFX.txt) |
| ArduinoJson | [6.21.5](https://github.com/bblanchon/ArduinoJson/tree/v6.21.5) | [MIT](licenses/ArduinoJson-MIT.txt) |
| Leaflet | [1.9.4](https://leafletjs.com/download.html) | [BSD-2-Clause](../assets/web/leaflet/LICENSE) |
| DejaVu fonts | Packed glyphs from the Home study font family | [Font notices](../assets/fonts/LICENSE-DejaVu.txt) |

The pinned Espressif32 6.12.0 toolchain/framework includes Arduino-ESP32 2.0.17
and ESP-IDF 4.4.7. Their dependency packages retain their own source/license
notices; see [Arduino-ESP32](https://github.com/espressif/arduino-esp32/tree/2.0.17)
and [ESP-IDF](https://github.com/espressif/esp-idf/tree/v4.4.7).
Manufacturer PDFs in reference/ retain their publishers' rights and are not
project-authored code. Public CA trust assets are described in firmware/certs/.
