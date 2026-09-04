/* rfid_module_code.ino : ruthsarian@gmail.com :v0.001
 * --
 * The purpose of this code is to provide a basic setup for reading data from common RFID modules
 *
 * References
 *   RFID modules: 
 *     RF125-PS: https://www.tindie.com/products/icstation/rfid-125khz-em4100-id-reader8295/
 *      RDM6300: https://handsontec.com/index.php/product/rdm6300-125khz-rfid-card-reader-module/
 *
 *   RDM6300 clones with broken firmware:
 *     https://arduino.ru/forum/apparatnye-voprosy/rfid-rdm6300-v40-poddelka-ne-rabotaet-kak-rdm6300-v03-resheno
 *
 * RFID Protocol for 'typical' RFID Modules:
 *
 *  0x02 + 10 bytes of tag data + 1 or 2 bytes of checksum + 0x03
 *
 *  0x02 = start of tag marker
 *  0x03 = end of tag marker
 *
 *  tag data is transmitted as ASCII encoded HEX values
 *  checksum is produced by XORing the 5 (binary) bytes of tag data
 *  
 *  some readers will deliver the checksum as a single, raw byte value
 *  others will encode the byte value in ASCII
 *
 *  some (bad) readers do not transmit start or end tags
 *  the one I know of that does this also does not transmit a checksum.
 *  it does, however, transmit a 0x20 after the 10 bytes of tag data, so we can use that.
 *
 *  there are some C functions that will decode ASCII HEX, but I just hacked something
 *  simple together that seems to work so let's go with that. 
 *
 */

#define RFID_SERIAL_RX_PIN    3     // for some devices this is fixed; Seeed XIAO:7, TrinketM0:3, RP2040 Zero:1
#define RFID_SERIAL_TX_PIN    -1    // disable TX
#define RFID_SERIAL_BAUDRATE  9600

// use hardware serial 1 for SAMD, ESP32, RP2040 platforms; adjust to taste
#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)
  #define RFID_SERIAL Serial1

// else, use software serial on supported platforms
#elif defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_ESP8266)
  #define RFID_SERIAL softSerial1
  #include <SoftwareSerial.h>
  SoftwareSerial softSerial1(RFID_SERIAL_RX_PIN, RFID_SERIAL_TX_PIN);

// else, we can't do serial.
#else
  #error "No supported serial interface for this board."
#endif

// just dumps the bytes, in hex, to serial as we read them in; used for debugging things
uint32_t rfid_read_bytes() {
  uint8_t c = 0;
  if (RFID_SERIAL.available()) {
    c = RFID_SERIAL.read();
    if (c < 0x10) {
      RFID_SERIAL.print("0");
    }
    Serial.print(c, HEX);
    Serial.print(" ");
  }
  return 0;
}

// read RFID tags over serial from the RFID reader module
// return the lower 16 bits, this is all we need for kyber crystals
// if you want the full 4 or 5 bytes you can mod this code as needed 
uint32_t rfid_read_id() {
  static bool bad_reader = false;
  static uint8_t data_pos = 0;
  static uint8_t data[5];
  static uint8_t checksum[2];
  static uint32_t last_read = 0;
  uint8_t c, i;
  uint16_t id;

  if (RFID_SERIAL.available()) { 
    c = RFID_SERIAL.read();
    last_read = millis();

    // start of data, clear data buffer
    if (data_pos == 0 && c == 2) {
      //Serial.println("START");
      for (i=0; i<5; i++) {
        data[i] = 0;
      }
      data_pos++;

    // read in tag ID which is presented as hex values in ASCII 
    // this code converts the ASCII to binary as we read in each byte
    //
    // i'm including a option here where if
    } else if ((data_pos <= 10 || (data_pos == 0)) && c > 47 && c < 71) {

      // there is a 'bad' version of the rdm6300 which does not transmit 0x02 and 0x03 nor any checksum
      // so we're going to do our best to still work with it
      //
      // that means the first byte will be data, not a start-of-data marker
      if (data_pos == 0) {
        for (i=0; i<5; i++) {
          data[i] = 0;
        }
        bad_reader == true;
        data_pos++;
      }

      //Serial.print("D:");
      //Serial.println(c);

      // convert ascii to binary for this nibble
      c = c - (c < 65 ? 48 : 55);

      // what position in data[] are we writing to?
      i = (uint8_t)((data_pos - 1) / 2);
      data[i] |= c << ((data_pos % 2 == 1) ? 4 : 0);

      data_pos++;

    // read in checksum byte(s)
    } else if (data_pos > 10 && c != 3 && !bad_reader) {
      //Serial.print("C:");
      //Serial.println(c);

      checksum[data_pos-11] = c;
      data_pos++;

    // end of tag received
    //
    // the end-of-tag marker is 0x03, however the checksum, if being sent as a binary value, could also be 0x03
    // when we get here, we've already recorded 1 byte of checksum (assuming we're not a 'bad' reader) so we
    // can assume a value of 0x03 is a legitimate end-of-tag marker
    } else if (c == 0x03 || (bad_reader && c == 0x20)) {
      //Serial.println("END");

      if (!bad_reader) {

        // some readers transmit checksum as ascii encoded hex value, others as the raw byte value
        //
        // if checksum is 1 byte long, it's a raw byte
        // if checksum is 2 bytes long, it's an ascii representation of the value in HEX
        //
        // if checksum is in ASCII convert to binary value and store back in checksum[0]
        if ((data_pos - 11) == 2) {
          c = (checksum[0] - (checksum[0] < 65 ? 48 : 55)) << 4 | (checksum[1] - (checksum[1] < 65 ? 48 : 55));
          checksum[0] = c;
        }

        // calculate checksum
        c = data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4];

        // verify checksum
        if (c != checksum[0]) {
          /*
          Serial.print("BAD CHECKSUM: ");
          Serial.print(c, HEX);
          Serial.print(" != ");
          Serial.println(checksum[0], HEX);
          */
          return(0);
        }
      } else {

        // reset bad_reader flag
        //
        // why? given we're likely only using a bad reader, why reset the flag?
        // because there's a chance we incorrectly 
        bad_reader = false;
      }

      // we're done using data_pos, reset it to 0 for the next tag
      data_pos = 0;

      // return the 32-bit tag ID
      id = data[4] | data[3] << 8 | data[2] << 16 | data[1] << 24;
      return(id);

      /*
      Serial.print("TAG: ");
      for (i=0; i<5; i++) {
        if (data[i] < 0x10) {
          Serial.print("0");
        }
        Serial.print(data[i], HEX);
      }
      Serial.println();
      */
    }

  // reset if we're in the middle of reading a tag and haven't received new data in a while
  } else if (data_pos != 0 && last_read < (millis() - 2000)) {
    //Serial.println("DATA RESET!");
    data_pos = 0;
    bad_reader = false;
  }
  return 0;
}

void kyber_react(uint16_t kyber_id) {

  switch (kyber_id) {
    case 3072:
    case 3080:
      Serial.println("White");
      break;
    case 3073:
    case 3081:
    case 3082:
    case 3085:
    case 3121:
      Serial.println("Red");
      break;
    case 3074: 
      Serial.println("Orange");
      break;
    case 3075:
    case 3083:
      Serial.println("Yellow");
      break;
    case 3076:
    case 3084:
    case 3122:
      Serial.println("Green");
      break;
    case 3077:
      Serial.println("Cyan");
      break;
    case 3078:
    case 3086:
      Serial.println("Blue");
      break;
    case 3079:
    case 3087: 
      Serial.println("Purple");
      break;
    case 3123: 
      Serial.println("Black");
      break;
    case 0:
      Serial.println("Crystal removed!");
      break;
    default:
      Serial.print("Unknown (");
      Serial.print(kyber_id);
      Serial.println(")");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // setup serial connection to RFID reader
  #ifdef ARDUINO_ARCH_ESP32
    RFID_SERIAL.begin(RFID_SERIAL_BAUDRATE, SERIAL_8N1, RFID_SERIAL_RX_PIN, RFID_SERIAL_TX_PIN);
  #else
    RFID_SERIAL.begin(RFID_SERIAL_BAUDRATE);
  #endif
}

void loop() {
  uint16_t id;
  static uint32_t last_read = 0;
  static uint32_t current_id = 0;

  // try to read a kyber crystal
  id = rfid_read_id();

  // do we have an rfid ID? 
  if (id != 0) {

    // is it a NEW ID?
    if (id != current_id) {

      // react to the new ID; for kyber crystals we only care about the lower 16 bits of the ID
      current_id = id;
      kyber_react((uint16_t)(current_id & 0xFFFF));
    }
    last_read = millis();
  }

  // reset the state if we haven't had a recent read
  if(current_id != 0 && last_read < millis() - 2500) {
    current_id = 0;
    kyber_react(0);
  }
}


