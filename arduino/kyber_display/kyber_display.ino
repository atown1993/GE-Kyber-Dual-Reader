/*  kyber_display.ino  --  GE-Kyber-Dual-Reader, step 2: OLED readout
 *  ------------------------------------------------------------------
 *  Reads a Galaxy's Edge kyber crystal on the RDM6300 and shows, on the
 *  board's onboard OLED:
 *      line 1  "Series 1 Kyber Crystal" / "Series 2 Kyber Crystal"
 *      line 2  crystal color (no character names -- see docs/crystal-id-map.md section 6)
 *  The full raw frame still prints to Serial (115200) so we can keep
 *  verifying the series marker across more crystals.
 *
 *  BOARD: classic ESP32 "HW-724" (HiLetgo 0.96" OLED, ESP-32D WROOM).
 *  Arduino IDE board = "ESP32 Dev Module", Serial Monitor 115200.
 *
 *  LIBRARY REQUIRED: "U8g2" by oliver  (Library Manager -> search U8g2).
 *
 *  PIN MAP (verified against this board's photos + pinout):
 *    OLED  : I2C, SDA=GPIO5, SCL=GPIO4, RST=GPIO16, addr 0x3C  (on-board)
 *    RDM6300 TX -> 10k/20k divider -> ESP32 SVP (GPIO36)  (UART2 RX)
 *    RDM6300 +5V -> ESP32 5V ; RDM6300 GND -> 2N7000 drain (source -> GND, gate -> GPIO0)
 *
 *  --- decode rule (docs/crystal-id-map.md) ---
 *  Frame data = 5 bytes: [version][tag hi..lo]. Character/color live in the
 *  low 12 bits of the tag (0xC00 + id). The VERSION byte carries the SERIES:
 *  0x00 = Series 1, 0x11 = Series 2 (verified 2026-09-03 across ~11 crystals).
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <HardwareSerial.h>

// U8g2 full-buffer, hardware I2C. Constructor args: rotation, reset, SCL, SDA.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /*reset=*/16, /*SCL=*/4, /*SDA=*/5);

HardwareSerial RFID(2);
const int  RFID_RX_PIN = 36;      // divider tap; matches step1_bringup
const int  FRAME_LEN   = 14;      // 0x02 + 10 data + 2 checksum + 0x03
const unsigned long HOLD_MS = 1500; // clear the screen this long after last read

char buf[FRAME_LEN];
int  idx = 0;
unsigned long lastRead = 0;
bool showingIdle = false;
uint32_t lastTagShown = 0xFFFFFFFF;

// ---- crystal ID table (docs/crystal-id-map.md), index = low nibble 0x0..0xF ----
// No character names -- the Series 2 character is not in the broadcast id (see
// docs/crystal-id-map.md section 6), so both series show SERIES + COLOR only.
const char* CRYSTAL_COLOR[16] = {
  "White","Red","Orange","Yellow","Green","Cyan","Blue","Purple",
  "White","Red","Red","Yellow","Green","Red","Blue","Purple"
};

uint8_t hex1(char c){
  if (c>='0'&&c<='9') return c-'0';
  if (c>='A'&&c<='F') return c-'A'+10;
  if (c>='a'&&c<='f') return c-'a'+10;
  return 0;
}
uint8_t hex2(char hi,char lo){ return (hex1(hi)<<4)|hex1(lo); }

void drawScreen(const char* series,const char* color){
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tr);         // "Series 1 Kyber Crystal" = 22 chars; 6x12 only fits 21
  oled.drawStr(0,14,series);
  oled.setFont(u8g2_font_ncenB14_tr);    // big color line
  oled.drawStr(0,44,color);
  oled.sendBuffer();
}

void drawIdle(){
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(0,26,"Present a crystal");
  oled.drawStr(0,42,"to the coil...");
  oled.sendBuffer();
}

const int READER_A_EN = 0;    // 2N7000 A gate. Held HIGH = reader A powered (TDM comes in step 3)

void setup(){
  pinMode(READER_A_EN, OUTPUT); digitalWrite(READER_A_EN, HIGH);
  Serial.begin(115200);
  oled.begin();
  RFID.begin(9600, SERIAL_8N1, RFID_RX_PIN, -1);   // -1 = no TX
  delay(200);
  drawIdle();
  Serial.println();
  Serial.println("kyber_display ready. Hold a crystal against the coil.");
}

void handleFrame(){
  // 5 data bytes from buf[1..10], checksum byte from buf[11..12]
  uint8_t data[5], chk = 0;
  for (int i=0;i<5;i++){ data[i]=hex2(buf[1+2*i],buf[2+2*i]); chk ^= data[i]; }
  uint8_t frameChk = hex2(buf[11],buf[12]);
  if (chk != frameChk) return;                 // reject garbage / two-tag collisions

  uint8_t  version = data[0];
  uint32_t tag = ((uint32_t)data[1]<<24)|((uint32_t)data[2]<<16)|
                 ((uint32_t)data[3]<<8)|data[4];
  uint16_t colorId = tag & 0x0FFF;             // low 12 bits carry color/character

  // ---- SERIES from the version byte ----
  const char* series;
  char seriesBuf[24];
  if      (version==0x00) series="Series 1 Kyber Crystal";
  else if (version==0x11) series="Series 2 Kyber Crystal";
  else { snprintf(seriesBuf,sizeof(seriesBuf),"Series? 0x%02X",version); series=seriesBuf; }

  // ---- color ----
  const char* color; char colorBuf[16];
  if (colorId>=0xC00 && colorId<=0xC0F)  color = CRYSTAL_COLOR[colorId & 0x0F];
  else if (colorId==0xC31)               color = "Red";     // special
  else if (colorId==0xC32)               color = "Green";   // special (8-ball Yoda)
  else if (colorId==0xC33)               color = "Black";   // special
  else { snprintf(colorBuf,sizeof(colorBuf),"? 0x%03X",colorId); color=colorBuf; }

  drawScreen(series,color);
  lastRead = millis();
  showingIdle = false;

  // ---- raw line to Serial for verification ----
  if (tag != lastTagShown){
    lastTagShown = tag;
    Serial.print("VERSION 0x"); if(version<16)Serial.print('0'); Serial.print(version,HEX);
    Serial.print("  colorId 0x"); Serial.print(colorId,HEX);
    Serial.print("  ("); Serial.print(series); Serial.print(", ");
    Serial.print(color); Serial.println(")");
  }
}

void loop(){
  while (RFID.available()){
    int v = RFID.read();
    if (v==0x02) idx=0;
    if (idx<FRAME_LEN) buf[idx++]=(char)v;
    if (v==0x03 && idx==FRAME_LEN){ handleFrame(); idx=0; }
  }
  if (!showingIdle && millis()-lastRead > HOLD_MS){
    drawIdle(); showingIdle=true; lastTagShown=0xFFFFFFFF;
  }
}
