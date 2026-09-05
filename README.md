# GE Kyber Dual Reader

A bench display that reads **both ends of a double-ended Galaxy's Edge kyber crystal at
once** — the series and color programmed into each tip, shown on a small OLED, with an
RGB LED at each end lit in the crystal's color. Single-ended crystals work too, and the
display tells you whether a crystal is **Series 1 or Series 2**. It deliberately shows no
character name: for Series 2 crystals the character isn't in the part of the tag this reader
can see ([why](docs/crystal-id-map.md#6-series-2--the-character-is-in-tag-memory-not-the-broadcast-id)), so both series get the same honest readout.

![Two-reader breadboard](docs/breadboard-step3.png)

Built from cheap, common parts: two **RDM6300** 125 kHz RFID modules with their stock coils,
one **ESP32** board with a built-in OLED, two MOSFETs, two RGB LEDs, a handful of resistors.
No custom antenna, no PCB, no soldering beyond tinning two coil leads. Roughly $65 in parts if you buy everything in packs (see the BOM); much less if you already have resistors, jumpers and a breadboard.

## What's here

| | |
|---|---|
| [`BUILD.md`](BUILD.md) | Parts list, the one pin map, wiring → flash → test, in order |
| [`EXPLAINER.md`](EXPLAINER.md) | How the crystals work, what the reader sees, what we measured, what didn't work |
| [`docs/crystal-id-map.md`](docs/crystal-id-map.md) | The tag → color table, the series marker, and why character names aren't shown |
| [`docs/GE-Kyber-Dual-Reader-BOM.xlsx`](docs/GE-Kyber-Dual-Reader-BOM.xlsx) | Bill of materials with Amazon links, quantities, pack sizes |
| [`docs/breadboard-step1.html`](docs/breadboard-step1.html) / [`.png`](docs/breadboard-step1.png) | Breadboard map, one reader (every wire stays put for step 3) |
| [`docs/breadboard-step3.html`](docs/breadboard-step3.html) / [`.png`](docs/breadboard-step3.png) | Breadboard map, the full two-reader build |
| [`docs/esp32-hw724-pins.html`](docs/esp32-hw724-pins.html) / [`.png`](docs/esp32-hw724-pins.png) | The ESP32 board pin by pin — which header position gets what |
| [`docs/wiring-modules.svg`](docs/wiring-modules.svg) / [`.png`](docs/wiring-modules.png) | Module-level wiring: ESP32 ↔ RDM6300 ↔ divider ↔ coil |
| `arduino/step1_bringup/` | Step 1: one reader, raw tag values on the Serial Monitor |
| `arduino/kyber_display/` | Step 2: one reader, series + color on the OLED |
| `arduino/kyber_dual/` | Step 3: two readers, both ends, LEDs |
| `reference/` | ruthsarian's single-reader `rfid_module_code.ino` (his code, his permission) |

Open the `.html` drawings in any browser; they're self-contained. The `.png` files are
the same drawings rendered.

## Credit

This project stands on **ruthsarian's** work for the Galaxy's Edge maker community: his
RDM6300 kyber sketches (the EM4100 frame format, the checksum scheme, and the
`0xC00 + id` decode rule — 2019 originals, and the 2026 multi-variant reader in
`reference/`, included with his permission) and his lightsaber research spreadsheets (the
crystal ID → character → color table). The sketches here are new code written for a
two-reader ESP32 design, but they would not exist without that groundwork. His GitHub:
<https://github.com/ruthsarian>.

The "pass-through" antenna idea in the explainer comes from charger06's Instructables
"DIY V2 Kyber Crystal RFID Identifier / Pass-Through Scanner".

## License

MIT — see [`LICENSE`](LICENSE). Build it, change it, sell it, just keep the notice.

*This is an unofficial fan project. It is not affiliated with, endorsed by, or connected to
Disney, Lucasfilm, or Galaxy's Edge. Character names appear only to identify which crystal
a tag is programmed as.*

*Amazon links in the BOM are affiliate links; they cost you nothing and help fund the next build.*
