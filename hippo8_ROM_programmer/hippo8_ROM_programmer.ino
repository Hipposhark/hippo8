/*
========================================================================
Creates the ROM for CPU ROM in memory
credits to DerULF and Ben Eater

writes to 28c256 EEPROMS

========================================================================
*/


// arduino pin constants
#define SHIFT_DATA   2
#define SHIFT_CLOCK  3
#define SHIFT_LATCH  4
#define EEPROM_D0    5
#define EEPROM_D7    12
#define WRITE_ENABLE 13

// 'set_AT28C256_address' variable subsitutes
#define READ         true
#define WRITE        false


// utility functions

void init_ports() {
  digitalWrite(WRITE_ENABLE, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLOCK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(WRITE_ENABLE, OUTPUT);
}


void set_data_pins_mode(int mode) { // pass in 'INPUT' or 'OUTPUT'
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    pinMode(pin, mode);
  }
}


void set_AT28C256_address(int address, boolean is_output_enabled) {
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, (address >> 8) | (is_output_enabled ? 0 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

void raw_write(int address, byte data) {
  set_data_pins_mode(OUTPUT);
  set_AT28C256_address(address, WRITE);

  byte curr_data = data;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    digitalWrite(pin, curr_data & 0x80);
    curr_data = curr_data << 1;
  }

  digitalWrite(WRITE_ENABLE, LOW);
  delayMicroseconds(20);
  digitalWrite(WRITE_ENABLE, HIGH);
  delay(20); // wait full write cycle (tWC in datasheet) (even if data isn't stored)
}


void enable_sdp_AT28C256() { // enable software data protection for at28c64b
  raw_write(0x5555, 0xAA);
  raw_write(0x2AAA, 0x55);
  raw_write(0x5555, 0xA0);
}

void write_to_AT28C256(int address, byte data) {
  byte data_read = read_from_AT28C256(address);
  if (data_read != data) {
    enable_sdp_AT28C256();

    set_data_pins_mode(OUTPUT);
    set_AT28C256_address(address, WRITE);

    byte curr_data = data;
    for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
      digitalWrite(pin, curr_data & 0x80);
      curr_data <<= 1;
    }

    digitalWrite(WRITE_ENABLE, LOW);
    delayMicroseconds(1);
    digitalWrite(WRITE_ENABLE, HIGH);
    delayMicroseconds(1000);


    // Immediately after write pulse
    byte read_back;
    unsigned long start_checking_time = millis();
    unsigned long time_passed         = 0;
    unsigned long curr_time;
    int           time_spent_checking = 0;
    
    do {
      read_back = read_from_AT28C256(address);

      curr_time = millis();
      time_spent_checking = (curr_time - start_checking_time) / 1000;
      if (time_spent_checking > time_passed) {
        time_passed = time_spent_checking;
        Serial.print(".");
      }

    } while ((read_back & 0x80) != (data & 0x80));  // Polling bit 7

    if (time_passed > 1) {
      Serial.println("");
    }

    // error checking
    // byte read_back = read_from_AT28C256(address);

    // if (read_back != data){
    //   char buffer[80];
    //   sprintf(buffer, "\r\nWRITE ERROR on 0x%04x! (want: %02x, read: %02x)\r\n", address, data, read_back);
    //   Serial.println(buffer);
    // }
  }
}


byte read_from_AT28C256(int address) {
  set_data_pins_mode(INPUT);

  set_AT28C256_address(address, READ);

  byte data_read = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
  {
    data_read = data_read * 2 + digitalRead(pin);
  }
  return data_read;
}


void print_256_bytes_from_AT28C256(int base_address) { // prints the next 256 bytes of data starting from the 'base_address'
  for (int base = base_address; base < base_address + 256; base += 16) {
    byte data[16];

    for (int offset = 0; offset < 16; offset++) {
      data[offset] = read_from_AT28C256(base + offset);
    }

    char buf[80];
    sprintf(buf, "%04x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8],
            data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);
  }
}

// Test programs

// counting up and down
const byte program_data[] PROGMEM = {
  /* 0x0000 */ 0x5c, 0x00, 0x02, 0x5d, 0x01, 0xdc, 0x03, 0x00, 0x5e, 0x01, 0xdd, 0x06, 0x00, 0xd6, 0x03, 0x00
};

void setup() {
  Serial.begin(57600);
  unsigned long start_time = millis();

  init_ports();
  Serial.println("starting!");
  print_256_bytes_from_AT28C256(0);

  // write_to_AT28C256(0xFFFC & 8191, 0xFF); // little endian
  // write_to_AT28C256(0xFFFD & 8191, 0xFF);

  // write_to_AT28C256(0xFFFC & 8191, 0x00); // little endian
  // write_to_AT28C256(0xFFFD & 8191, 0xE0);

  // print_256_bytes_from_AT28C256(0xFEFF & 8191);

  // Serial.println("wrote to reset vector!");

  // int a = 0x0000;
  // int i = 0;
  // while (i < sizeof(program_data)) {
  //   byte bb = __LPM((uint16_t)(program_data + i));
    
  //   //
  //   char buf[80];
  //   sprintf(buf, "%04x: %02x", a, bb);
  //   Serial.println(buf);
    
  //   i++;

  //   write_to_AT28C256(a++, bb);
  //   delay(1);
  // }
  // delay(1);
  // print_256_bytes_from_AT28C256(0);

}


// void setup() {
//   Serial.begin(57600);
//   unsigned long start_time = millis();

//   // test reset vector

//   init_ports();
//   Serial.println("starting!");

//   write_to_AT28C256(0xFFFC & 8191, 0x00); // little endian
//   write_to_AT28C256(0xFFFD & 8191, 0xE0);

//   print_256_bytes_from_AT28C256(0xFEFF & 8191);


//   Serial.println("wrote to reset vector!");


//   int a = 0;
//   int i = 0;
//   while (i < sizeof(program_data)) {
//     byte bb = __LPM((uint16_t)(program_data + i));
//     i++;
//     write_to_AT28C256(a++, bb);
//     delay(1);
//   }

//   // write_to_AT28C256(0, 0x5c);
//   print_256_bytes_from_AT28C256(0);
//   //

//   unsigned long end_time   = millis();
//   int           time_spent = (end_time - start_time) / 1000;
//   char          buf[80];
//   sprintf(buf, "took %d seconds", time_spent);
//   Serial.println(buf);
//   Serial.println("done.");
// }

void loop() {
  // put your main code here, to run repeatedly:

}
