# Kyber Crystal ID Map

Canonical lookup table for Galaxy's Edge kyber crystal RFID tags, cross-verified
across two independent sources (see **Sources** at the bottom).

Status: **16 base IDs verified 16/16** (Series 1 characters). 3 "special" IDs identified
but their individual assignment is unconfirmed. **Series 2 characters are NOT in the
broadcast ID — see §6.**

**Live-scan verification (2026-09-03, RDM6300 on HW-724):** version byte = series
(00=S1, 11=S2) confirmed across S1 {0xC01,0xC02,0xC07,0xC09,0xC0A,0xC32} and
S2 {0xC00,0xC03,0xC06,0xC07}. Mace Windu 0xC07 seen in BOTH series. NEW FINDING:
a physical 0xC02 (id 2) crystal exists (see §3). Specials: 0xC32 came back as S1.

---

## 1. The decode rule

Every kyber crystal carries a **125 kHz LF, EM4100-format** RFID tag. The tag
value read by an RDM6300 is:

```
tag_value = 0xC00 + crystal_id        (crystal_id = 0x0 .. 0xF)
```

That is, the whole base crystal set occupies decimal **3072-3087** (0xC00-0xC0F).
The low nibble IS the 4-bit crystal ID that Savi's Workshop hilts and holocrons
use internally. Nothing else in the tag matters for identification.

Verified by mapping all 16 `case` values in ruthsarian's `kyber_reader.ino`
against the ID column of his research spreadsheet — 16 of 16 agree.

> **Do not confuse this 4-bit ID with the legacy-hilt ID.** Legacy character
> hilts (Kylo Ren, Mace Windu, ...) have no crystal chamber. They send their own
> 4-bit identifier over the same wire, in the same 0-15 range, with a *different*
> meaning. The two spaces collide numerically and mean different things. Any
> table that lists "Legacy Hilts" next to a crystal ID is showing you which
> *hilt* shares that number, not which crystal.

---

## 2. Base table (IDs 0x0-0xF)

`Crystal Color` is the physical color of the crystal you hold. `Voice` is the
character the holocron associates with it. `Savi's Blade` is what a Savi's
Workshop hilt actually drives the blade to.

| Tag (dec) | Tag (hex) | ID | Crystal Color | Voice / Character | Savi's Blade | Savi's Clash |
|---|---|---|---|---|---|---|
| 3072 | 0xC00 | 0 | White | Ahsoka Tano | White | Yellow |
| 3073 | 0xC01 | 1 | Red | Darth Vader | Red | Orange |
| 3074 | 0xC02 | 2 | Orange *(no official Disney crystal; aftermarket exists)* | none | Orange → shown as Yellow | White |
| 3075 | 0xC03 | 3 | Yellow | Temple Guard | Yellow | White |
| 3076 | 0xC04 | 4 | Green | Qui-Gon Jinn | Green | Yellow |
| 3077 | 0xC05 | 5 | Cyan/Teal *(no official Disney crystal; aftermarket exists)* | none | Cyan → shown as Blue | (Blue) |
| 3078 | 0xC06 | 6 | Blue | Old Obi-Wan | Blue | Yellow |
| 3079 | 0xC07 | 7 | Purple | Mace Windu #1 | Purple | Yellow |
| 3080 | 0xC08 | 8 | White | Chirrut Îmwe | Dark Purple *(Orange on newer blades)* | Orange |
| 3081 | 0xC09 | 9 | Red | Palpatine | Red | Orange |
| 3082 | 0xC0A | A | Red | Count Dooku | Red | Orange |
| 3083 | 0xC0B | B | Yellow | Maz Kanata | Yellow | White |
| 3084 | 0xC0C | C | Green | Yoda | Green | Yellow |
| 3085 | 0xC0D | D | Red | Darth Maul | Red | Orange |
| 3086 | 0xC0E | E | Blue | Old Luke | Blue | Yellow |
| 3087 | 0xC0F | F | Purple | Mace Windu #2 | Purple | Yellow |

Note the shape: **physical color repeats, character does not.** Four red crystals
(Vader, Palpatine, Dooku, Maul), two white, two yellow, two green, two blue, two
purple. The character is the distinguishing payload, which is exactly why the tag
matters — you cannot tell Dooku from Maul by looking at the crystal.

---

## 3. The orange / cyan question — answered

> **CLARIFIED BY LIVE SCAN — 2026-09-03 (RESOLVED).** Matt scanned a physical
> **orange** crystal reading `0xC02` (ID 2) and has a double-ended crystal with a
> **teal/cyan** end (ID 5). These are **aftermarket** — third-party (Etsy) makers
> program crystals to the reserved orange/cyan slots. Disney's own claim below
> stands: Disney never *officially released* those colors. But physical,
> programmable crystals for IDs 0x2 and 0x5 **do** exist and should stay in the
> color table — they read and decode fine. Not a gap to chase; expected behavior.

**There is no official orange crystal and no official cyan (teal) crystal.**
IDs 0x2 and 0x5 are real, reserved slots in the 16-value color table, and Savi's
hilts will correctly emit those color codes if such a tag is presented — but
Disney never released a physical crystal carrying either ID.

This is why the count works out: 16 base slots − 2 unreleased (orange, cyan)
+ 3 specials = **17 known crystal RFIDs**, which is exactly the number
ruthsarian's spreadsheet claims.

Two further wrinkles, both from the Communications workbook:

- A **stock V2 blade** does not render those colors even when commanded. Its
  internal color table translates **cyan → blue** and **orange → yellow**. The
  hilt is doing the right thing; the blade is the limitation.
- You can mod a stock blade to render orange and cyan, but only by *swapping*
  them in for yellow and blue — at which point real yellow and blue crystals
  render as orange and cyan instead. It's a rotation, not an addition. The only
  clean fix is replacing the blade's microcontroller.

**Implication for this project:** we are not bound by the stock blade's color
table. Driving our own LEDs, ID 0x2 can be genuinely orange and 0x5 genuinely
cyan. `kyber_reader.ino` already treats them that way.

---

## 4. The three "specials" — partially resolved

Tag values **3121, 3122, 3123** (0xC31, 0xC32, 0xC33) sit outside the base 16.
ruthsarian names the three specials as **Snoke, 8-ball Vader, and 8-ball Yoda**,
but does not state which tag belongs to which.

What ruthsarian's 2026 `rfid_module_code.ino` (`reference/`) renders for them —
this supersedes the 2019 sketch, which had 0xC33 as red:

| Tag (dec) | Tag (hex) | Rendered color | Likely crystal (UNVERIFIED) |
|---|---|---|---|
| 3121 | 0xC31 | Red | Snoke or 8-ball Vader |
| 3122 | 0xC32 | Green | 8-ball Yoda |
| 3123 | 0xC33 | **Black** | one of the two Sith specials |

0xC32 → 8-ball Yoda is a reasonably safe call on color alone (it's the only green
of the three). 0xC33 = Black comes from ruthsarian's newer code, not from a scan
in this project; `kyber_dual.ino` lights it dim white so "present" still shows.
Pinning names to 0xC31 / 0xC33 needs a physical scan of a known crystal. **Open item.**

---

## 5. Known gaps

1. **Post-2022 crystals are unaudited.** Both source workbooks predate any
   crystal released after ruthsarian's research window. The tag space between
   0xC10 and 0xC30 is entirely unexamined, and new releases would most plausibly
   land there or above 0xC33. Anything scanned that isn't in this document is a
   finding, not an error — log it.
2. ~~**The version field is discarded.**~~ **RESOLVED 2026-09-03 — the version
   byte is the SERIES marker.** We captured it on a live RDM6300: **`0x00` = Series 1,
   `0x11` = Series 2.** Confirmed across multiple crystals, incl. Mace Windu `0xC07`
   read as both `v00` (S1) and `v11` (S2). So the full decode is: `colorId = tag &
   0xFFF` (→ 0xC00+id, color/character per §2) AND the version byte → series. This is
   read straight from the EM4100 broadcast — no addressed memory read needed, which
   retires the "need an EM4095 for Series 2" assumption. (Whether `0x11`'s two nibbles
   or the tag's matching high byte encode anything finer is still open — log more S2
   samples.) **But see §6: the S2 CHARACTER is not in the broadcast at all.**
3. **Individual special-crystal assignment** (§4).
4. **Series 2 character identification** needs an EM4305 memory read (§6).

---

## 6. Series 2 — the character is in tag memory, not the broadcast ID

**Found 2026-09-04.** A Series 2 purple crystal sold as **General Grievous** scanned as
`VERSION 0x11 colorId 0xC07` — byte-identical on the wire to a Series 2 **Mace Windu**.
The S1 "voice" column in §2 does not apply to S2 crystals, and no S2 column can be
built from the broadcast ID at all.

Full EM4305 memory dumps of 17 Series 2 crystals (the "Kyber Crystal" Series 2 tab of
ruthsarian's collected Galaxy's Edge research workbook — community-shared data) show why. Every S2 crystal of a given color
shares one EM tag ID (`11 000C0x` — version `0x11` + the §1 color id), but the
**character lives in word 09**, high byte (low three bytes are a constant `0D0000`):

| Word 09 (hi byte) | S2 voice | Broadcast color id |
|---|---|---|
| 0x01 | Darth Sidious | 0xC01 (black crystal) |
| 0x02 | Ben Solo | 0xC06 (blue, "cracked") |
| 0x04 | Plo Koon | 0xC06 (blue) |
| 0x05 | Luke Skywalker | 0xC04 (green) |
| 0x06 | General Grievous | 0xC07 (purple) |
| 0x07 | Mace Windu | 0xC07 (purple) |
| 0x08 | Asajj Ventress | 0xC01 (red) |
| 0x11 | Maul | 0xC01 (red) |
| 0x13 | Grand Inquisitor | 0xC01 (red) |
| 0x14 | Krin Dagbard | 0xC00 (white) |
| 0x17 | Rey Skywalker | 0xC03 (yellow) |
| 0x18 | Luminara Unduli | 0xC04 (green) |

Other words: 06 varies with color (`0C80`/`2980`/`6F80`/`7B00`/`1800`/`5E00` + `3000`),
01 and 03 look per-tag unique (serial), the rest are constant across all 17.

**Consequence for this build:** the RDM6300 only decodes the EM4100 broadcast, so it
can report an S2 crystal's **series and color** but structurally cannot tell Grievous
from Mace Windu. `kyber_dual.ino` therefore shows "S2: char in tag mem" for any
`0x11` crystal instead of a wrong S1 name. Reading word 09 needs an addressed EM4305
read (an EM4095-class read/write front end, or a Proxmark) — a different reader, not a
firmware change. Note also that the S2 **black** crystal broadcasts as `0xC01` (red id),
not `0xC33`.

---

## Sources

All of the underlying research is **ruthsarian's** (Galaxy's Edge maker community):

- His 2026 `rfid_module_code.ino` (`reference/`, included with his permission) — the
  current reference parser and the specials table above.
- His 2019 RDM6300 sketches (`kyber_reader.ino`, 2019-08-26; LCD/servo fork by kochiro,
  2022-04-25) — original source of the raw tag values and the `0xC00 + id` rule. Not
  redistributed here (their own provenance is mixed, per ruthsarian).
- His Galaxy's Edge lightsaber research spreadsheets ("Lightsaber Blade Hardware and
  Operation" — the "Blade Command & Kyber Crystal IDs" section; "Lightsaber Hilt & Blade
  Communications" — the V2 blade-controller color table and the orange/cyan note).
  Shared with the community; not redistributed here.
- ruthsarian on GitHub: https://github.com/ruthsarian

The **version byte = series** finding (§ top) and the live-scan table are original to
this project (2026-09-03), measured with the sketches in `arduino/`.
