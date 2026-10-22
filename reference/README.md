# Hardware references

Public manufacturer documents downloaded October 20, 2026.

| Local document | Manufacturer source | Relevant sections |
| --- | --- | --- |
| [CC1101 datasheet, TI SWRS061I (Rev. I)](CC1101-TI-SWRS061I-datasheet.pdf) | [Texas Instruments](https://www.ti.com/lit/ds/symlink/cc1101.pdf) | Operating voltage; Table 4 receive current (printed page 10); asynchronous OOK reception. |
| [PanelLan ZX2D80CE02S / WT32S3-28S PRO datasheet](PanelLan-ZX2D80CE02S-WT32S3-28S-PRO-datasheet.pdf) | [Manufacturer PDF](https://img03.71360.com/w3/77q677/20250701/255a7c29ee6c23af4b5a41406aa43ffc.pdf) | Printed page 4, Tables 1–2: debug and expansion connectors. Debug pin 2 is +3.3 V; available output current is unspecified. |

The CC1101 document describes TI's chip. The owner's module header pinout is
recorded separately from their photo in [the wiring guide](../firmware/WIRING.md).
Module connector numbering is not the TI chip's package pin numbering.

See [the enclosure reference](ENCLOSURE.md) for the exact-board CAD search
and the display's mechanical drawing on printed page 9 of its datasheet.

[Archived audio concept](FUTURE_AUDIO.md) applies only to a possible future
larger-display version; sound is excluded from the current PanelLan device.
