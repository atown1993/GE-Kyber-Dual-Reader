# reference/

`rfid_module_code.ino` — **ruthsarian's** single-reader RFID sketch (v0.001, 2026-03-04),
included verbatim and unmodified. It is his code, not part of this project's MIT grant.

What it does that this project's sketches borrow from: handles genuine RDM6300 modules,
RDM6300 clones that send a 1-byte raw checksum instead of 2 ASCII-hex chars, and the
RF125-PS; picks hardware serial on SAMD/ESP32/RP2040 and SoftwareSerial on AVR/ESP8266;
maps the kyber IDs (incl. the three specials) to colors.

Permission, quoted from ruthsarian (Discord DM, 2026-09-04):

> i do have newer code here [...] i wrote that code entirely myself. if you want to include
> that as a reference that's totally fine. that code is also written to be compatible with a
> couple different RDM6300 variants and other similar modules.

His 2019 `kyber_reader.ino` / `kyber_colors_lcd.ino` are deliberately **not** included, at
his request — parts of them came from other RDM6300 examples he found online and did not cite.

Known nit in v0.001: line `bad_reader == true;` is a comparison, not an assignment, so the
no-STX/ETX "bad reader" path never engages. Reported to him; left as-is here.

ruthsarian on GitHub: https://github.com/ruthsarian
