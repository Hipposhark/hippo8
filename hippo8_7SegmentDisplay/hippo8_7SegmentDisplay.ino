/*
=============================================================================
IMPORTANT!!! this program writes to a AT28C64B! not the AT28C64 cuz I accidentally got the B version, its more complicated to implement grrrr :(


Code that writes a AT28C64B EEPROM driving a 4-digit 7-segment display output via th 74HC595 shift register
(inspired by Ben Eater and DerULF1)

allows output to display 8-bit data in 4 formats (unsigned decimal, signed decimal/2's complement hexadecimal, binary)

Corresponging numbers to segment of the 7-segment display for reference:

    111
   6   2
   6   2
    777
   5   3
   5   3
    444 8

=============================================================================
*/ 


/*
defining hardware constants
- we will shift out the address lines
- we will loop through the data lines to access all of them via the arduino (from 5 to 12)
*/ 
#define SHIFT_DATA   2  
#define SHIFT_CLOCK  3
#define SHIFT_LATCH  4  // controls 74HC595 Storage register clock pin (latch pin)
#define EEPROM_D0    5  // 
#define EEPROM_D7    12
#define WRITE_ENABLE 13

// modes for the 'set_eeprom_address' function
#define READ  true  // if we want to read  from the eeprom, then we have to enable  the output of the eeprom
#define WRITE false // if we want to write to   the eeprom, then we have to disable the output of the eeprom

// indices of the '-' and 'h' symbol in the 'symbols_bytes' array
#define SIGN_DIGIT 16 // '-'
#define HEX_DIGIT  17 // 'h'

// indices offset of the 'symbols_bytes' array
#define BIN_OFFSET 18

/**/
String SYMBOLS[22] = {
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  "A", "b", "C", "d", "E", "F", 
  "-", "h", "0b00", "0b01", "0b10", "0b11"
};

/*
array containing the maps describing the required segments to compose each of the corresponding 22 symbols
- the '0' character terminates the sequence
*/
int SYMBOLS_SEGMENTS[22][8] = {
  {1,2,3,4,5,6,0},   // 0
  {2,3,0},           // 1
  {1,2,4,5,7,0},     // 2
  {1,2,3,4,7,0},     // 3
  {2,3,6,7,0},       // 4
  {1,3,4,6,7,0},     // 5
  {1,3,4,5,6,7,0},   // 6
  {1,2,3,0},         // 7
  {1,2,3,4,5,6,7,0}, // 8
  {1,2,3,4,6,7,0},   // 9
  {1,2,3,5,6,7,0},   // A
  {3,4,5,6,7,0},     // b
  {1,4,5,6,0},       // C
  {2,3,4,5,7,0},     // d
  {1,4,5,6,7,0},     // E
  {1,5,6,7,0},       // F
  {7,0},             // -
  {3,5,6,7,0},       // h

  // we store 2 bits on the left and right horizontal segments of a 7-segment display
  {3,5,0},           // 0b00
  {2,3,5,0},         // 0b01
  {3,5,6,0},         // 0b10
  {2,3,5,6,0}        // 0b11
};

/*
=======================
UTILITY FUNCTIONS
=======================
*/

byte* get_symbol_bytes() { // returns the array containing the necessary bytes to write into the EEPROM
  static byte bytes[22];   // bytes corresponding to the symbols to be written into the EEPROM
  char   buffer[20];       // for sprintf

  for ( int symbol_i = 0; symbol_i < 22; symbol_i++ ) {
    int segment_i   = 0; // the index of the segment in each symbol mapping
    int symbol_byte = 0; //
    while ( SYMBOLS_SEGMENTS[symbol_i][segment_i] > 0 ) {
      int num_shifts = SYMBOLS_SEGMENTS[symbol_i][segment_i] - 1; // shifts a bit to its place in the byte
      symbol_byte   |= ( 1 << num_shifts);                       // update 'symbol_byte' by oring it with the new segment's corresponding byte place
      segment_i ++;                                              // increment 'segment_i'
    }

    bytes[symbol_i] = symbol_byte;                               // updates the 'bytes' array
    // String symbol   = SYMBOLS[symbol_i] + String(" ", 4 - SYMBOLS[symbol_i].length());
    // sprintf(buffer, "Symbol %d: %x", symbol_i, symbol_byte);
    String symbol   = SYMBOLS[symbol_i];
    sprintf(buffer, "Symbol %-4s: %x", symbol.c_str(), symbol_byte);
    Serial.println(buffer); 
  }

  return bytes;
}

void init_pins() { // initializes the arduino pins
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLOCK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(WRITE_ENABLE, OUTPUT);
  digitalWrite(WRITE_ENABLE, HIGH);  
  digitalWrite(SHIFT_LATCH, LOW);
}

void set_data_pins_mode(int new_pin_mode) { // sets the I/O mode of the arduino data pins (for reading vs. writing from the eeprom)
  for ( int pin_num = EEPROM_D7; pin_num >= EEPROM_D0; pin_num--) {
    pinMode(pin_num, new_pin_mode);
  }

}

/*
feeds the specified 'address' into the eeprom, and either enables or disables the eeprom output depending on if you're reading from or writing to the eeprom

ex: 
set_eeprom_address(1234, true) --> 00000100 11010010 --> 10000100 11010010 outputed 
*/
void set_eeprom_address(int address, bool output_enable) { //
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, (address >> 8) | (output_enable ? 0x00 : 0x80)); // shift out bits ahead of the first 8 bits (presumeably less than 8 bits); sets msb high if 'output_enable' is true, else sets the msb false
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, address);         // shift out last 8 bits

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

byte read_from_eeprom(int address) {
  set_data_pins_mode(INPUT);
  set_eeprom_address(address, READ);
  
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    data = data * 2 + digitalRead(pin);
  }

  return data;
}

void write_to_eeprom(int address, byte data) {
  set_data_pins_mode(OUTPUT); // we are sending data out of the data pins
  set_eeprom_address(address, WRITE);
  
  byte curr_data = data;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    digitalWrite(pin, curr_data & 0x80);
    curr_data = curr_data << 1;
  }
  digitalWrite(WRITE_ENABLE, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_ENABLE, HIGH);

  // error checking
  byte l = read_from_eeprom(address);
  int cnt = 1000;
  while ((cnt > 0) && (l != data)) {
    cnt--;
    if (address == 0)
      l = read_from_eeprom(256);
    else 
      l = read_from_eeprom(0);
    l = read_from_eeprom(address);
  }

  if (cnt == 0){
    char buf[80];
    sprintf(buf, "\r\nWRITE ERROR on %04x! (%02x %02x)\r\n", address, data, l);
    Serial.println(buf);
  }

}

void print_byte_from_eeprom(int base_address) { // prints the next 255 addresses of data starting from the 'base_address'
  for (int base = base_address; base < base_address + 256; base += 16) {
    byte data[16];

    for (int offset = 0; offset < 16; offset ++) {
      data[offset] = read_from_eeprom(base + offset);
    }

    char buf[80];
    sprintf(buf, "%04x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
      base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8],
      data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);

  }
}

// helper functions
int get_converted_decimal(int num) { // for getting digits of a signed decimal; pass in an 8-bit int
  int converted_decimal = num;
  if (num > 127) {
    converted_decimal = 256 - num;
  }
  return converted_decimal;
}

void setup() {
  // initializing stuff
  Serial.begin(57600);
  Serial.println("writing into eeprom");
  unsigned long start_time = millis();

  init_pins();
  byte* symbols_bytes = get_symbol_bytes();

  /* 
  ===========================
  WRITING PROCEDURE 
  - we will step through the different modes
  ===========================
  */

  /* writing symbols for unsigned mode */

  Serial.println("writing decimal unsigned 1st digit ");
  int base_address = 0;
  for (int i = 0; i <= 255; i++) {
    int ones = i % 10;
    write_to_eeprom(base_address + i, symbols_bytes[ones]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing decimal unsigned 2nd digit ");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int tens = (i / 10) % 10;
    write_to_eeprom(base_address + i, symbols_bytes[tens]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing decimal unsigned 3rd digit ");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int hundreds = (i / 100) % 10;
    write_to_eeprom(base_address + i, symbols_bytes[hundreds]);
  }
  print_byte_from_eeprom(base_address);
  
  Serial.println("writing decimal unsigned 4th digit (no digit)");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    write_to_eeprom(base_address + i, 0);
  }
  print_byte_from_eeprom(base_address);


  // writing symbols for signed mode

  Serial.println("writing decimal signed 1st digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int ones = get_converted_decimal(i) % 10;
    write_to_eeprom(base_address + i, symbols_bytes[ones]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing decimal signed 2nd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int tens = (get_converted_decimal(i) / 10) % 10;
    write_to_eeprom(base_address + i, symbols_bytes[tens]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing decimal signed 3rd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int hundreds = (get_converted_decimal(i) / 100) % 10;
    write_to_eeprom(base_address + i, symbols_bytes[hundreds]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing decimal signed 4th digit (maybe a sign)");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    bool yes_sign = i > 127;
    write_to_eeprom(base_address + i, yes_sign ? symbols_bytes[SIGN_DIGIT] : 0);
  }
  print_byte_from_eeprom(base_address);


  // writing symbols for hexidecimal mode

  Serial.println("writing hexadecimal 1st digit (hex marker)");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    write_to_eeprom(base_address + i, symbols_bytes[HEX_DIGIT]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing hexadecimal 2nd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int ones = i % 16;
    write_to_eeprom(base_address + i, symbols_bytes[ones]);
  }
  print_byte_from_eeprom(base_address);

  Serial.println("writing hexadecimal 3rd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int sixteenths = i / 16;
    write_to_eeprom(base_address + i, symbols_bytes[sixteenths]);
  }
  print_byte_from_eeprom(base_address);
  
  Serial.println("writing hexadecimal 4th digit (no digit)");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    write_to_eeprom(base_address + i, 0);
  }
  print_byte_from_eeprom(base_address);


  // writing symbols for binary mode
  
  Serial.println("writing binary 1st digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int first_two_bits = i & 3;
    write_to_eeprom(base_address + i, symbols_bytes[first_two_bits + BIN_OFFSET]);
  }
  print_byte_from_eeprom(base_address);
  
  Serial.println("writing binary 2nd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int second_two_bits = (i >> 2) & 3;
    write_to_eeprom(base_address + i, symbols_bytes[second_two_bits + BIN_OFFSET]);
  }
  print_byte_from_eeprom(base_address);
  
  Serial.println("writing binary 3rd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int third_two_bits = (i >> 4) & 3;
    write_to_eeprom(base_address + i, symbols_bytes[third_two_bits + BIN_OFFSET]);
  }
  print_byte_from_eeprom(base_address);
  
  Serial.println("writing binary 3rd digit");
  base_address += 256;
  for (int i = 0; i <= 255; i++) {
    int fourth_two_bits = (i >> 6) & 3;
    write_to_eeprom(base_address + i, symbols_bytes[fourth_two_bits + BIN_OFFSET]);
  }
  print_byte_from_eeprom(base_address);
  

  // 
  unsigned long end_time   = millis();
  int           time_spent = (end_time - start_time) / 1000;
  char          buf[80];
  sprintf(buf, "took %d seconds", time_spent);
  Serial.println(buf);
  Serial.println("done.");
}

// void setup() {
//   init_pins();
//   Serial.begin(57600);
//   Serial.println("testing ");

//   print_byte_from_eeprom(0);
//   for (int i = 0; i < 256; i ++) {
//     write_to_eeprom(i, 0);
//   }
//   print_byte_from_eeprom(0);

//   // byte* symbols_bytes = get_symbol_bytes();
//   // for (int i = 0; i < 22; i ++){
//   //   Serial.println(symbols_bytes[i]);
//   // }

//   // int base_address = 0;
//   // for (int i = 0; i < 16; i ++) {
//   //   print_byte_from_eeprom(i * 256 + base_address);
//   //   Serial.println(i);
//   // }
// }

void loop() {
  // put your main code here, to run repeatedly:

}
