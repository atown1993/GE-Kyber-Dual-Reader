# How it works — and what we learned building it

## The crystal

Every Galaxy's Edge kyber crystal has a small **125 kHz LF RFID tag** inside, EM4100-format.
Powered by the reader's field, it broadcasts a fixed ID over and over. That ID is what a
Savi's Workshop hilt reads to pick the blade color and character voice.

An **RDM6300** module decodes that broadcast and sends a 14-byte serial frame at 9600 baud:

```
0x02 | 10 ASCII-hex chars: 2 = version, 8 = tag | 2 chars checksum | 0x03
```

Decode: `colorId = tag & 0xFFF`, and the base crystals are `0xC00 + id` with `id` = 0..15.
The 16 IDs map to character + color exactly as ruthsarian documented in 2019
(`docs/crystal-id-map.md`).

**Finding: the version byte is the series marker.** Every earlier sketch discarded those
first two hex chars. On a live reader they're `00` on every Series 1 crystal and `11` on
every Series 2 crystal we scanned (~11 crystals, including Mace Windu `0xC07` seen in both
series). This matters because the community assumption was that Series 2 needed an
*addressed* read of the tag's memory (EM4305 word 9), which a broadcast-only RDM6300 can't
do — implying a pricier EM4095-class module and a custom antenna. Not so: the cheap module
reads the series straight from the broadcast, with its stock coil.

## The double-ended problem

A double-ended crystal has **two tags, one at each tip, ~35 mm apart**. The hilt reads
only the nearer one; flip the crystal, get the other character. To show both at once you
need two readers that each see only their own end.

EM4100 has **no anti-collision**. Two tags in one field both modulate at once; the reader
either decodes the louder one or gets garbage (which fails the checksum and is dropped —
a nice property: it fails to *silence*, never to a wrong answer).

What we measured with one stock RDM6300 coil and an orange/teal double-ended crystal:

| Test | Result |
|---|---|
| Crystal held end-on, tip toward the coil | Reads the **near** end every time; flip → other end. Zero wrong answers over ~11 flips. |
| Crystal ~25 mm above the coil, flat | Reads (generous range) |
| Crystal laid flat inside the loop, 1 minute | Reads whichever tag wins first, **never flips** — deterministic |
| Crystal pushed through the loop | Reads leading tag, then trailing tag, in order — motion changes the coupling |

Conclusions: the near tag wins by a wide margin end-on, so **two stock coils facing each
other with the crystal standing between them** discriminate the ends without any custom
antenna. And there is no software trick to get a single coil to "try for the other tag" —
only geometry or motion changes the answer.

## Why the readers take turns (TDM)

Each RDM6300 radiates a continuous 125 kHz carrier. Two coils facing each other an inch
apart hear each other's carrier far louder than any tag. So the firmware **time-division
multiplexes**: a 2N7000 MOSFET on each reader's ground lets the ESP32 power one reader at
a time — A for a second, then B, forever. Each end's LED holds its last good read, so to
your eye both are lit continuously. The modules' own green LEDs blinking alternately with
nothing nearby is the visible sign it's working.

Measured: an RDM6300 delivers its first frame **405–471 ms** after its window opens, so
the per-reader turn is set to 1000 ms (two chances per turn). Whether the readers would also
work with both powered continuously at 25 mm is untested.

## Provenance by wire

Which reader saw the crystal is never inferred from data: reader A is on one hardware UART
(SVP / GPIO36), reader B on another (SVN / GPIO39). A frame on UART1 came from the bottom
coil, full stop.

## The ESP32 board's pin budget

The HW-724 (a Wemos LOLIN32-OLED clone) exposes only 13 usable GPIOs — four header pins are
the flash chip's, two are input-only, and two (0, 12) are boot-strapping pins. The build
needs 10: two serial inputs, two MOSFET gates, six LED PWM channels. The map in `BUILD.md`
is the only arrangement that fits: input-only pins take the serial lines, the boot pins take
the MOSFET gates (a gate is a harmless load at reset; an LED cathode is not — it drags the
pin low and GPIO0-low means download mode), and the six remaining pins take the LEDs.

## LED brightness

Common-anode RGB LEDs from a 3.3 V rail: the red die drops ~2 V, leaving 1.3 V across its
resistor — 220 Ω gives a healthy 6 mA. Green and blue dies drop ~3 V, leaving ~0.3 V; at
100 Ω that's 3 mA and teal is barely visible. **47 Ω** on green/blue fixed it. Don't go
below ~22 Ω.

## What's not done

- An enclosure. The geometry is known (coils ~25 mm apart, crystal standing end-on); the
  print isn't designed.
- A full-collection scan. Series 2 crystals couple more weakly than Series 1 and were the
  first to drop out in our interference tests.
- Whether the two `0x11` nibbles or the tag's matching high byte encode anything finer than
  "Series 2".

## Credits

- **ruthsarian** — the 2019 RDM6300 sketches, the frame/checksum handling, the crystal ID
  research. <https://github.com/ruthsarian>
- **charger06** — the pass-through scanner idea (Instructables, "DIY V2 Kyber Crystal RFID
  Identifier / Pass-Through Scanner").
- Galaxy's Edge maker community on Discord, for the antenna and EM4305 discussion that framed
  the questions this build answered.
