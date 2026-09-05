/*  kyber_dual.ino  --  GE-Kyber-Dual-Reader, step 3: TWO readers + RGB LEDs
 *  ------------------------------------------------------------------------
 *  Reads BOTH ends of a double-ended kyber crystal with two RDM6300 modules,
 *  one coil at each end, and lights a plain 4-pin common-anode RGB LED per end
 *  in the crystal's color. OLED shows TOP / BOTTOM name + color.
 *
 *  BOARD: classic ESP32 "HW-724" (HiLetgo 0.96" OLED). IDE board = "ESP32 Dev
 *  Module", Serial Monitor 115200. LIBRARY: U8g2 (same as kyber_display).
 *
 *  WHY TIME-DIVISION (TDM): each RDM6300 radiates a continuous 125 kHz carrier.
 *  Two coils facing each other ~40 mm apart couple hard, and reader A's carrier
 *  is far louder at coil B than any tag reply. So only ONE reader is powered at
 *  a time: its ground is switched by a 2N7000 N-channel MOSFET (low-side
 *  switch). Firmware: power A -> read window -> latch -> power B -> ... Each
 *  LED holds its last good color between windows. Set TDM_ENABLED 0 to run both
 *  readers continuously for an A/B comparison on the bench.
 *
 *  WHICH READER READ? Provenance is by WIRE: reader A (top) is on UART2, reader
 *  B (bottom) on UART1. A frame on UART1 came from the bottom coil. Period.
 *
 *  PIN MAP -- HW-724 pinout VERIFIED from board photo 2026-09-03 (matches the
 *  Wemos LOLIN32-OLED diagram; see docs/esp32-hw724-pins.html). This board exposes only 13 usable
 *  GPIOs: 25,26 + SVP(36),SVN(39) on the left; 16,5,4,0,2,14,12,13,15 on the right
 *  (RX/TX are the USB console; SD1/CMD/SD0/CLK are flash -- never touch).
 *    OLED (on-board)   SDA=GPIO5  SCL=GPIO4  RST=GPIO16  addr 0x3C   -- reserved
 *    Reader A (TOP)    TX -> 10k/20k divider -> SVP = GPIO36 (input-only, UART2 RX)
 *    Reader B (BOTTOM) TX -> 10k/20k divider -> SVN = GPIO39 (input-only, UART1 RX)
 *    MOSFET A gate     GPIO0    (2N7000 drain = reader A GND, source = ESP GND)
 *    MOSFET B gate     GPIO12   (2N7000 drain = reader B GND, source = ESP GND)
 *    LED TOP  (common anode -> 3V3)   R=GPIO25  G=GPIO26  B=GPIO2    via resistors
 *    LED BOT  (common anode -> 3V3)   R=GPIO13  G=GPIO14  B=GPIO15   via resistors
 *    Resistors: 220 ohm on each R leg, 47 ohm on each G and B leg (100 was too dim
 *    on a 3.3 V rail -- green/blue dies drop ~3 V; bench-verified 2026-09-03).
 *  Why these and not others: GPIO0 and GPIO12 are boot-strapping pins. A MOSFET
 *  gate is a harmless load on them (no pull either way), but an LED cathode is
 *  NOT (it drags the pin low at reset: GPIO0 low = download mode, GPIO12 high =
 *  wrong flash voltage). So gates go on 0/12, LEDs on the rest. GPIO36/39 are
 *  input-only, which is exactly what a UART RX needs.
 *
 *  COMMON ANODE means the ESP32 SINKS current: GPIO LOW = LED on. The LEDC PWM
 *  duty is therefore inverted (255 - brightness). Anode goes to 3V3, NOT 5V
 *  (a 5 V anode against a 3.3 V "off" pin leaves ~1.7 V across a red LED and
 *  it glows faintly, and pushes current into the pin).
 *
 *  Decode rule (docs/crystal-id-map.md): version byte 0x00 = Series 1,
 *  0x11 = Series 2; colorId = tag & 0xFFF, 0xC00 + character id.
 *
 *  FRAME VARIANTS: genuine RDM6300 (and the HW-205 this was built on) sends the
 *  checksum as 2 ASCII-hex chars; some clones send it as 1 raw byte. Both are
 *  accepted (see pump()). Clones that send NO STX/ETX/checksum at all are not
 *  supported -- the checksum is what rejects two-tag collisions in this design.
 *  Variant handling follows ruthsarian's reference/rfid_module_code.ino.
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <HardwareSerial.h>

// ---------------- tunables ----------------
#define TDM_ENABLED   1        // 1 = alternate readers via the 2N7000 ground switches (BENCH-VERIFIED
                               //     2026-09-03: reads both ends, coils ~25 mm apart). 0 = both always on (untested).
const unsigned long WINDOW_MS   = 1000;  // per-reader turn. Measured first-frame = 405-471 ms, so 500 was too short
const uint8_t       MISS_LIMIT  = 3;     // windows with no valid read before an end goes dark
const uint8_t       COMMON_ANODE = 1;    // 1 = common anode (GPIO sinks), 0 = common cathode
const unsigned long FRAME_IDLE_MS = 500; // a half-received frame older than this is discarded

// ---------------- pins ----------------
const int PIN_RX_A = 36, PIN_RX_B = 39;   // SVP, SVN
const int PIN_EN_A = 0,  PIN_EN_B = 12;
const int LED_TOP[3] = {25, 26, 2};      // R, G, B
const int LED_BOT[3] = {13, 14, 15};

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /*reset=*/16, /*SCL=*/4, /*SDA=*/5);
HardwareSerial RFID_A(2);
HardwareSerial RFID_B(1);

// ---------------- crystal tables ----------------
// Character per crystal id -- SERIES 1 ONLY. Series 2 crystals (version byte 0x11)
// reuse the same 16 color ids but the S2 character is NOT in the broadcast id: it
// lives in tag memory (EM4305 word 09), which an RDM6300 cannot read. Two S2
// crystals with identical broadcast ids can be different characters (0xC07 = Mace
// Windu AND General Grievous). So for S2 we report the color and say so.
// See docs/crystal-id-map.md section 6.
const char* CHAR_NAME_S1[16] = {
  "Ahsoka Tano","Darth Vader","(orange)","Temple Guard",
  "Qui-Gon Jinn","(teal)","Old Obi-Wan","Mace Windu #1",
  "Chirrut Imwe","Palpatine","Count Dooku","Maz Kanata",
  "Yoda","Darth Maul","Old Luke","Mace Windu #2"
};
const char* CRYSTAL_COLOR[16] = {
  "White","Red","Orange","Yellow","Green","Teal","Blue","Purple",
  "White","Red","Red","Yellow","Green","Red","Blue","Purple"
};
// RGB per character id (0..15). Tune to taste on the bench.
const uint8_t CRYSTAL_RGB[16][3] = {
  {255,255,255},{255,0,0},{255,70,0},{255,170,0},
  {0,255,0},{0,255,120},{0,0,255},{170,0,255},
  {255,255,255},{255,0,0},{255,0,0},{255,170,0},
  {0,255,0},{255,0,0},{0,0,255},{170,0,255}
};

// ---------------- per-reader state ----------------
struct Reader {
  const char*     label;     // "TOP" / "BOT"
  HardwareSerial* port;
  int             enPin;
  const int*      led;       // R,G,B pins
  int             ledcBase;  // LEDC channels base..base+2
  char            buf[14];
  int             idx;
  // latched result
  bool            present;
  uint8_t         version;
  uint16_t        colorId;
  uint8_t         misses;
  bool            readThisWindow;
  unsigned long   firstFrameMs;   // diagnostics: time from power-on to first valid frame
  // "distinct IDs seen recently" -- answers "if the whole crystal sits in ONE
  // coil, does the winner ever flip?" (Matt, 2026-09-03). Up to 4 IDs, 5 s window.
  uint16_t        seenId[4];
  unsigned long   seenAt[4];
  unsigned long   lastByteMs;     // for the stale half-frame reset
};
Reader A = {"TOP", &RFID_A, PIN_EN_A, LED_TOP, 0, {0},0, false,0,0,0,false,0, {0},{0}, 0};
Reader B = {"BOT", &RFID_B, PIN_EN_B, LED_BOT, 3, {0},0, false,0,0,0,false,0, {0},{0}, 0};

const char* charName(uint16_t colorId, uint8_t version){   // below struct Reader on purpose: Arduino 1.8 inserts ALL auto-prototypes at the first function
  if (version==0x11) return "S2: char in tag mem";   // not readable over EM4100 broadcast
  return CHAR_NAME_S1[colorId & 0xF];
}

uint8_t hex1(char c){
  if (c>='0'&&c<='9') return c-'0';
  if (c>='A'&&c<='F') return c-'A'+10;
  if (c>='a'&&c<='f') return c-'a'+10;
  return 0;
}
uint8_t hex2(char hi,char lo){ return (hex1(hi)<<4)|hex1(lo); }

// ---------------- LEDs ----------------
// The ESP32 Arduino core changed its PWM (LEDC) API in v3.0: channels went away,
// you attach a pin directly. Both forms are here so the sketch compiles on
// whichever core is installed (Tools -> Board -> Boards Manager shows the version).
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void ledSetup(Reader& r){ for (int i=0;i<3;i++) ledcAttach(r.led[i], 5000, 8); }      // 5 kHz, 8-bit
void ledWrite(Reader& r, uint8_t red, uint8_t grn, uint8_t blu){
  uint8_t v[3] = {red, grn, blu};
  for (int i=0;i<3;i++) ledcWrite(r.led[i], COMMON_ANODE ? 255 - v[i] : v[i]);
}
#else
void ledSetup(Reader& r){
  for (int i=0;i<3;i++){ ledcSetup(r.ledcBase+i, 5000, 8); ledcAttachPin(r.led[i], r.ledcBase+i); }
}
void ledWrite(Reader& r, uint8_t red, uint8_t grn, uint8_t blu){
  uint8_t v[3] = {red, grn, blu};
  for (int i=0;i<3;i++) ledcWrite(r.ledcBase+i, COMMON_ANODE ? 255 - v[i] : v[i]);
}
#endif
void ledShow(Reader& r){
  if (!r.present){ ledWrite(r,0,0,0); return; }
  if (r.colorId>=0xC00 && r.colorId<=0xC0F){
    const uint8_t* c = CRYSTAL_RGB[r.colorId & 0xF];
    ledWrite(r, c[0], c[1], c[2]);
  } else if (r.colorId==0xC31){ ledWrite(r,255,0,0); }                       // Snoke / 8-ball Vader (red)
  else if (r.colorId==0xC32){ ledWrite(r,0,255,0); }                          // 8-ball Yoda (green)
  else if (r.colorId==0xC33){ ledWrite(r,20,20,20); }                         // Black (per ruthsarian 2026 code; unscanned) -- dim so "present" still shows
  else ledWrite(r, 40, 40, 40);                                               // unknown: dim white
}

// ---------------- OLED ----------------
void describe(const Reader& r, char* out, size_t n){
  if (!r.present){ snprintf(out,n,"%s: --", r.label); return; }
  const char* name; const char* color; char tmp[16];
  if (r.colorId>=0xC00 && r.colorId<=0xC0F){ name=charName(r.colorId,r.version); color=CRYSTAL_COLOR[r.colorId&0xF]; }
  else if (r.colorId==0xC31){ name="Snoke/8ball"; color="Red"; }
  else if (r.colorId==0xC32){ name="8ball Yoda"; color="Green"; }
  else if (r.colorId==0xC33){ name="Special"; color="Black"; }
  else { snprintf(tmp,sizeof(tmp),"0x%03X",r.colorId); name=tmp; color="?"; }
  snprintf(out,n,"%s: S%d %s", r.label, r.version==0x11?2:1, color);
  // second line holds the name; caller draws it
  (void)name;
}
const char* nameOf(const Reader& r){
  if (!r.present) return "";
  if (r.colorId>=0xC00 && r.colorId<=0xC0F) return charName(r.colorId,r.version);
  if (r.colorId==0xC31) return "Snoke/8ball Vader";
  if (r.colorId==0xC32) return "8-ball Yoda";
  if (r.colorId==0xC33) return "Special (black)";
  return "Unknown";
}
void drawScreen(const Reader* active){
  char l1[24], l3[24];
  describe(A,l1,sizeof(l1)); describe(B,l3,sizeof(l3));
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(0,11,l1);  oled.drawStr(0,23,nameOf(A));
  oled.drawHLine(0,30,128);
  oled.drawStr(0,43,l3);  oled.drawStr(0,55,nameOf(B));
  // tiny activity marker: which reader is powered right now
  oled.drawStr(110,64, active==&A ? "A" : active==&B ? "B" : "AB");
  oled.sendBuffer();
}

// ---------------- reader power ----------------
void powerReader(Reader& r, bool on){
  digitalWrite(r.enPin, on ? HIGH : LOW);     // 2N7000 gate high = ground connected = ON
  if (on){
    while (r.port->available()) r.port->read();  // flush anything queued while it was off
    r.idx = 0;
    r.readThisWindow = false;
    r.firstFrameMs = 0;
  }
}

// ---------------- distinct-ID tracker ----------------
const unsigned long SEEN_WINDOW_MS = 5000;
void noteSeen(Reader& r, uint16_t id){
  unsigned long now = millis(); int slot=-1, oldest=0;
  for (int i=0;i<4;i++){
    if (r.seenAt[i] && now-r.seenAt[i] > SEEN_WINDOW_MS){ r.seenAt[i]=0; }   // expire
    if (r.seenAt[i] && r.seenId[i]==id){ r.seenAt[i]=now; return; }
    if (!r.seenAt[i]) slot=i; else if (r.seenAt[i]<r.seenAt[oldest]) oldest=i;
  }
  if (slot<0) slot=oldest;
  r.seenId[slot]=id; r.seenAt[slot]=now;
  int n=0; for (int i=0;i<4;i++) if (r.seenAt[i]) n++;
  if (n>1){                                   // more than one ID inside the window: the winner flipped
    Serial.printf("[%s] MULTIPLE IDs in last %lus:", r.label, SEEN_WINDOW_MS/1000);
    for (int i=0;i<4;i++) if (r.seenAt[i]) Serial.printf(" 0x%03X", r.seenId[i]);
    Serial.println();
  }
}

// ---------------- frame handling ----------------
bool handleFrame(Reader& r, unsigned long windowStart){
  uint8_t data[5], chk = 0;
  for (int i=0;i<5;i++){ data[i]=hex2(r.buf[1+2*i],r.buf[2+2*i]); chk ^= data[i]; }
  // idx==14: 2-char ASCII checksum (genuine RDM6300); idx==13: 1 raw byte (some clones)
  uint8_t frameChk = (r.idx==14) ? hex2(r.buf[11],r.buf[12]) : (uint8_t)r.buf[11];
  if (chk != frameChk) return false;                         // garbage / two-tag collision
  uint32_t tag = ((uint32_t)data[1]<<24)|((uint32_t)data[2]<<16)|((uint32_t)data[3]<<8)|data[4];
  if (tag == 0) return false;                                // the 0x000 ghost frame seen 2026-09-03
  uint8_t version = data[0];
  uint16_t colorId = tag & 0x0FFF;

  noteSeen(r, colorId);
  bool changed = (!r.present) || r.colorId!=colorId || r.version!=version;
  r.present = true; r.version = version; r.colorId = colorId; r.misses = 0;
  if (!r.readThisWindow){ r.readThisWindow = true; r.firstFrameMs = millis()-windowStart; }
  if (changed){
    Serial.printf("[%s] VERSION 0x%02X colorId 0x%03X (S%d, %s, %s)  first-frame %lu ms after power-on\n",
      r.label, version, colorId, version==0x11?2:1, nameOf(r),
      (colorId>=0xC00&&colorId<=0xC0F)?CRYSTAL_COLOR[colorId&0xF]:"?", r.firstFrameMs);
  }
  return true;
}
void pump(Reader& r, unsigned long windowStart){
  while (r.port->available()){
    int v = r.port->read();
    r.lastByteMs = millis();
    if (v==0x02) r.idx=0;
    if (r.idx<14) r.buf[r.idx++]=(char)v;
    // ETX lands at idx 14 (ASCII checksum) or 13 (raw checksum byte). A 0x03 arriving
    // at idx 12 IS a raw checksum byte whose value happens to be 3 -- keep going.
    if (v==0x03 && (r.idx==14 || r.idx==13)){ handleFrame(r, windowStart); r.idx=0; }
  }
  if (r.idx>0 && millis()-r.lastByteMs > FRAME_IDLE_MS) r.idx=0;   // half-frame went stale
}
void endWindow(Reader& r){
  if (!r.readThisWindow){
    if (r.present && ++r.misses >= MISS_LIMIT){
      r.present = false;
      Serial.printf("[%s] cleared (no valid read for %d windows)\n", r.label, MISS_LIMIT);
    }
  }
}

// ---------------- setup / loop ----------------
void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.printf("kyber_dual build %s %s -- S2 char: tag-memory only\n", __DATE__, __TIME__);   // proves which build is actually running
  pinMode(PIN_EN_A, OUTPUT); pinMode(PIN_EN_B, OUTPUT);
  digitalWrite(PIN_EN_A, LOW); digitalWrite(PIN_EN_B, LOW);
  ledSetup(A); ledSetup(B); ledShow(A); ledShow(B);
  oled.begin();
  RFID_A.begin(9600, SERIAL_8N1, PIN_RX_A, -1);
  RFID_B.begin(9600, SERIAL_8N1, PIN_RX_B, -1);
  delay(200);
  Serial.println();
  Serial.printf("kyber_dual ready. TDM=%d window=%lu ms. A=TOP(SVP/36) B=BOT(SVN/39)\n", TDM_ENABLED, WINDOW_MS);
#if !TDM_ENABLED
  powerReader(A,true); powerReader(B,true);
#endif
  drawScreen(nullptr);
}

void loop(){
#if TDM_ENABLED
  static Reader* order[2] = {&A, &B};
  for (int k=0;k<2;k++){
    Reader& r = *order[k];
    Reader& other = *order[1-k];
    powerReader(other,false);
    powerReader(r,true);
    unsigned long t0 = millis();
    drawScreen(&r);
    while (millis()-t0 < WINDOW_MS){
      pump(r, t0);
      // The OFF reader's TX floats when its ground is cut and the divider picks up
      // junk bytes. Expected (seen 2026-09-03); discard silently. Nothing is latched from it.
      while (other.port->available()) other.port->read();
      delay(2);
    }
    endWindow(r);
    ledShow(A); ledShow(B);
  }
#else
  static unsigned long t0 = millis();
  pump(A,t0); pump(B,t0);
  if (millis()-t0 >= WINDOW_MS){ endWindow(A); endWindow(B); A.readThisWindow=B.readThisWindow=false; t0=millis(); ledShow(A); ledShow(B); drawScreen(nullptr); }
  delay(2);
#endif
}
