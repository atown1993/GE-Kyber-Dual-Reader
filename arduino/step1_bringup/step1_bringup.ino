/*  step1_bringup.ino  --  GE-Kyber-Dual-Reader, Step 1: one reader, raw tags
 *  ------------------------------------------------------------------
 *  Goal: put a kyber crystal near the RDM6300's stock coil and see its
 *  tag number on the Serial Monitor. Nothing else -- no LEDs, no second
 *  reader, no enclosure. This answers the two open questions:
 *    1. Does the RDM6300 read your crystals -- BOTH Series 1 and Series 2?
 *    2. What is in the "version" byte ruthsarian's sketch throws away?
 *
 *  Board:  classic ESP32 (HiLetgo ESP32 OLED), "ESP32 Dev Module".
 *  Serial Monitor: 115200 baud.  RDM6300 link is fixed at 9600.
 *
 *  Wiring (docs/breadboard-step1.html has every hole):
 *    RDM6300 +5V  -> ESP32 5V/VIN
 *    RDM6300 GND  -> 2N7000 DRAIN; 2N7000 SOURCE -> ESP32 GND; GATE -> GPIO0
 *                    (the MOSFET goes in now so nothing is rewired in step 3)
 *    RDM6300 TX   -> 10k --+-- 20k -> GND   divider; junction -> SVP (GPIO36)
 *    RDM6300 RX   -> nothing (read-only)
 *    ANT1 / ANT2  -> the two coil leads (no polarity)
 *  The coil leads are ENAMELLED -- scrape + tin 3-5 mm or the module
 *  looks completely dead for a purely mechanical reason.
 *
 *  ESP32 GPIOs are NOT 5V tolerant. The divider (5V * 20/30 = 3.33V) is
 *  mandatory -- never wire RDM6300 TX straight to a GPIO.
 */

#include <HardwareSerial.h>

// UART2 on the ESP32. UART0 is the USB console, so the reader gets UART2.
// Real hardware UART -- no SoftwareSerial, no timing bugs.
HardwareSerial RFID(2);

const int RFID_RX_PIN = 36;   // SVP (GPIO36, input-only): the reader's FINAL pin, nothing
                              // moves in later steps. Not 16/5/4 -- the on-board OLED owns those.
const int FRAME_LEN   = 14;   // 0x02 + 10 data + 2 checksum + 0x03

char buf[FRAME_LEN];
int  idx = 0;

const int READER_A_EN = 0;    // 2N7000 A gate. Held HIGH = reader A powered (TDM comes in step 3)

void setup() {
  pinMode(READER_A_EN, OUTPUT); digitalWrite(READER_A_EN, HIGH);
  Serial.begin(115200);
  RFID.begin(9600, SERIAL_8N1, RFID_RX_PIN, -1);   // -1 = no TX pin needed
  delay(300);
  Serial.println();
  Serial.println("RDM6300 ready. Hold a crystal against the coil.");
}

void loop() {
  while (RFID.available()) {
    int v = RFID.read();

    if (v == 0x02) idx = 0;               // start of frame, restart buffer
    if (idx < FRAME_LEN) buf[idx++] = (char)v;

    if (v == 0x03 && idx == FRAME_LEN) {  // complete frame
      dumpFrame();
      idx = 0;
    }
  }
}

void dumpFrame() {
  // Frame layout, all ASCII hex chars between the 0x02 and 0x03 markers:
  //   buf[0]      = 0x02 start marker
  //   buf[1..2]   = version   (2 hex chars = 1 byte)
  //   buf[3..10]  = tag       (8 hex chars = 4 bytes)
  //   buf[11..12] = checksum  (2 hex chars = 1 byte)
  //   buf[13]     = 0x03 end marker
  // No checksum gate on purpose: during bring-up a failed frame still
  // proves the reader is seeing SOMETHING. Add the check once reads are
  // reliable.
  char ver[3], tag[9];
  memcpy(ver, buf + 1, 2); ver[2] = '\0';
  memcpy(tag, buf + 3, 8); tag[8] = '\0';

  long tagval = strtol(tag, NULL, 16);

  Serial.print("VERSION: ");    Serial.print(ver);
  Serial.print("   TAG hex: "); Serial.print(tag);
  Serial.print("   TAG dec: "); Serial.print(tagval);
  Serial.print("   (0x");       Serial.print(tagval, HEX);
  Serial.println(")");
}
