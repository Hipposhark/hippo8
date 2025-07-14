/*
========================================================================
Creates the ROMS driving the Instruction Decoder & Control Logic
heavily inspired by DerULF's implementation
also credit to Ben Eater

datasheet links
- 74HC595    (shift registers)   : https://www.ti.com/lit/ds/symlink/sn74hc595.pdf
- AT28C64    (microcode EEPROMS) : https://ww1.microchip.com/downloads/en/devicedoc/doc0001h.pdf
- SST39SF040 (label EEPROMS)     : https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/SST39SF010A-SST39SF020A-SST39SF040-Data-Sheet-DS20005022.pdf

========================================================================
*/

/* defining constants */

// arduino pin constants
#define SHIFT_DATA 2
#define SHIFT_CLOCK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_ENABLE 13

// 'set_eeprom_address' variable subsitutes
#define READ true
#define WRITE false
#define CHIP_ENABLE true   // for SST39SF040 chip enable
#define CHIP_DISABLE false // for SST39SF040 chip enable

// ROM chip reference definitions
#define CURR_ROM_NUM 0    // 0, 1, 2, 3
#define FIRST_LABEL_ROM 4 // 4, 5
#define IS_LABEL_ROM (CURR_ROM_NUM >= FIRST_LABEL_ROM)
#define IS_MCODE_ROM !IS_LABEL_ROM

// 'write_label_byte' variable substitutes
#define IS_OPCODE true            // variable substitues for 'is_an_opcode'
#define IS_PREDEFINED_LABEL false // variable substitues for 'is_an_opcode'

// microcode ROM 0 Control Lines
#define _HLT 1                  // halt system clock
#define _PCE ((uint32_t)1 << 1) // program counter enable
#define _PCW ((uint32_t)1 << 2) // program counter write
#define _PCC ((uint32_t)1 << 3) // program counter count
#define _SPE ((uint32_t)1 << 4) // stack pointer enable
#define _SPW ((uint32_t)1 << 5) // stack pointer write
#define _SPD ((uint32_t)1 << 6) // stack pointer direction/decrement (0 = up, 1 = down)
#define _SPC ((uint32_t)1 << 7) // stack pointer count

// microcode ROM 1 Control Lines
#define _TRE ((uint32_t)1 << 8)  // transfer register enable
#define _TLW ((uint32_t)1 << 9)  // transfer lower write
#define _TUW ((uint32_t)1 << 10) // transfer upper write
#define _MMW ((uint32_t)1 << 11) // memory write
#define _CIC ((uint32_t)1 << 12) // control instruction cycle
#define _CIL ((uint32_t)1 << 13) // control instruction load
#define _CTI ((uint32_t)1 << 14) // control toggle interrupt
#define _PSW ((uint32_t)1 << 15) // port selector write

// microcode ROM 2 Control Lines
#define _PSS ((uint32_t)1 << 16) // port selector selection
#define _RAW ((uint32_t)1 << 17) // register A write
#define _RBW ((uint32_t)1 << 18) // register B write
#define _RCW ((uint32_t)1 << 19) // register C write
#define _RDW ((uint32_t)1 << 20) // register D write
#define _OTW ((uint32_t)1 << 21) // register O (output) write
#define _REW ((uint32_t)1 << 22) // register E (accumulator) write
#define _RFW ((uint32_t)1 << 23) // register F (flags) write

// microcode ROM 3 Control Lines
#define _MX0 ((uint32_t)1 << 24) // data bus output MUX control line 0
#define _MX1 ((uint32_t)1 << 25) // data bus output MUX control line 1
#define _MX2 ((uint32_t)1 << 26) // data bus output MUX control line 2
#define _MX3 ((uint32_t)1 << 27) // data bus output MUX control line 3
#define _AZS ((uint32_t)1 << 28) // ALU control line ZS
#define _AZ0 ((uint32_t)1 << 29) // ALU control line Z0
#define _AZ1 ((uint32_t)1 << 30) // ALU control line Z1
#define _AZ2 ((uint32_t)1 << 31) // ALU control line Z2

// MUX'd data bus output control lines
#define _PCL ((uint32_t)1 << 24)  // program counter lower byte enable
#define _PCU ((uint32_t)2 << 24)  // program counter upper byte enable
#define _SPL ((uint32_t)3 << 24)  // stack pointer upper byte enable
#define _SPU ((uint32_t)4 << 24)  // stack pointer lower byte enable
#define _TRL ((uint32_t)5 << 24)  // transfer register lower byte enable
#define _TRU ((uint32_t)6 << 24)  // transfer register upper byte enable
#define _MME ((uint32_t)7 << 24)  // memory enable
#define _PSE ((uint32_t)8 << 24)  // port selector enable
#define _RAE ((uint32_t)9 << 24)  // register A enable
#define _RBE ((uint32_t)10 << 24) // register B enable
#define _RCE ((uint32_t)11 << 24) // register C enable
#define _RDE ((uint32_t)12 << 24) // register D enable
#define _REE ((uint32_t)13 << 24) // register E (accumulator) enable
#define _RFE ((uint32_t)14 << 24) // register F (flags) enable
// #define  ((uint32_t)15 << 24)

// ALU related control lines (if _REW is active)
#define _ALU_x00 ((uint32_t)0)          // load (x00) into reg. E
#define _ALU_SUB (_AZ0)                 // load (reg. E - data bus) into reg. E
#define _ALU_ADD (_AZ0 | _AZ1)          // load (reg. E + data bus) into reg. E
#define _ALU_XOR (_AZ2)                 // load (reg. E XOR data bus) into reg. E
#define _ALU_OR (_AZ0 | _AZ2)           // load (reg. E OR data bus) into reg. E
#define _ALU_AND (_AZ1 | _AZ2)          // load (reg. E AND data bus) into reg. E
#define _ALU_xFF (_AZ0 | _AZ1 | _AZ2)   // load (xFF) into reg. E
#define _ALU_BUS (_AZ0 | _AZS)          // load data bus into reg. E (_RFW is inactive)
#define _ALU_LSL (_AZ1 | _AZS)          // load left-shifted data bus into reg. E
#define _ALU_LSR (_AZ0 | _AZ1 | _AZS)   // load right-shifted data bus into reg. E
#define _ALU_CMP (_AZ1 | _AZ2 | _AZS)   // compare data bus to reg. E and load results into reg. F (if _RFW is active and _REW is inactive)

// ALU's flag manipulation related control lines (if _RFW is active)
#define _FLG_BUS (_AZ0 | _AZS)        // load data bus into reg. F
#define _FLG_CLC (_AZ2 | _AZS)        // clear carry flag (b0000XXX0)
#define _FLG_STC (_AZ0 | _AZ2 | _AZS) // set carry flag   (b0000XXX1)

// ALU and control logic dependent flags
#define _F_C ((int)1)   // ALU 'carry'    flag
#define _F_Z ((int)2)   // ALU 'zero'     flag
#define _F_N ((int)4)   // ALU 'negative' flag
#define _F_V ((int)8)   // ALU 'overflow' flag
#define _F_II ((int)16) // 1 = interrupt inhibited
#define _F_IR ((int)32) // 1 = interrupt requested
#define _F_IF ((int)64) // 1 = instruction fetch

// creates the active low control lines bitfield
#define _negatives (_PCE | _PCW | _PCC | _SPE | _SPC | _TRE | _TLW | _TUW | _MMW | _CIC | _CIL | _PSW | _PSS | _RAW | _RBW | _RCW | _RDW | _REW | _RFW)

int fetch_address = 0;     // define start address of the fetch cycle microcode
int interrupt_address = 0; // define start address of the intterupt routine microcode

// register arrays, used to create register related microcode via loops
const char     GP_REGISTERS[] = {'A', 'B', 'C', 'D'};     // define the mapping of the general purpose registers
const uint32_t GP_REG_WRITE[] = {_RAW, _RBW, _RCW, _RDW}; // define the mapping of the generla purpose registers write  lines
const uint32_t GP_REG_ENBLE[] = {_RAE, _RBE, _RCE, _RDE}; // define the mapping of the generla purpose registers enable lines

int num_write_errors = 0; // tracks the number of write errors to the eeprom

/* defining basic helper functions */

void init_ports() {
  digitalWrite(WRITE_ENABLE, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLOCK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(WRITE_ENABLE, OUTPUT);
}

void set_data_pins_mode(int mode) { // pass in 'INPUT' or 'OUTPUT'
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
  {
    pinMode(pin, mode);
  }
}

void set_SST39SF040_address(int address, boolean is_output_enabled, boolean is_chip_enabled) {
  digitalWrite(SHIFT_LATCH, LOW);

  // 19-bit address space (19 pins) + out enable (1 pin) + chip enable (1 pin) = 21 pins ~ 3 bytes
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, (address >> 16) | (is_output_enabled ? 0 : 0x80) | (is_chip_enabled ? 0 : 0x40)); // shifts top    byte and adds output enable and chip enable bits
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, (address >> 8));                                                                  // shifts middle byte
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, address);                                                                         // shifts bottom byte

  digitalWrite(SHIFT_LATCH, HIGH);
}

void write_byte_to_SST39SF040(int address, byte data) { // directly writes a byte given an address
  set_data_pins_mode(OUTPUT);
  set_SST39SF040_address(address, WRITE, CHIP_ENABLE);
  digitalWrite(WRITE_ENABLE, LOW);

  byte curr_data = data;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
  {
    digitalWrite(pin, curr_data & 0x80); // write the leftmost bit, which evaluates to HIGH or LOW
    curr_data <<= 1;
  }
  delayMicroseconds(1);
  digitalWrite(WRITE_ENABLE, HIGH);
}

void write_data_to_SST39SF040(int address, byte data) { // writes data to eeprom given an address, conforming to the data program protocol
  /*
  Byte Program Operation (from the datasheet):
  1. The first step is the three-byte load sequence for
  Software Data Protection.
  2. The second step is to load byte address and
  byte data. During the Byte Program operation,
  the addresses are latched on the falling edge of
  either CE# or WE#, whichever occurs last. The
  data are latched on the rising edge of either CE#
  or WE#, whichever occurs first.
  3. The third step is the internal Program operation,
  which is initiated after the rising edge of the
  fourth WE# or CE#, whichever occurs first.
  */

  // step 1
  write_byte_to_SST39SF040(0x5555, 0xAA);
  write_byte_to_SST39SF040(0x2AAA, 0x55);
  write_byte_to_SST39SF040(0x5555, 0xA0);

  // step 2 & step 3 (performed internally)
  write_byte_to_SST39SF040(address, data);

  // error correction step
  byte l = read_from_SST39SF040(address);
  int cnt = 1000;
  while ((cnt > 0) && (l != data))
  {
    cnt--;
    set_SST39SF040_address(address, WRITE, CHIP_DISABLE);
    l = read_from_SST39SF040(address);
  }

  if (cnt == 0)
  {
    char buf[60];
    sprintf(buf, "\r\nWRITE ERROR on %06x! (%02x %02x)\r\n", address, data, l);
    Serial.println(buf);
    num_write_errors++;
  }
}

byte read_from_SST39SF040(int address) {
  set_data_pins_mode(INPUT);
  set_SST39SF040_address(address, READ, CHIP_ENABLE);
  delayMicroseconds(1);

  byte data_read = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
  {
    data_read = data_read * 2 + digitalRead(pin);
  }

  return data_read;
}

void erase_sector_from_SST39SF040(unsigned long base_address) {
  // TODO
}

void erase_SST39SF040() {
  // TODO
}

void print_256_bytes_from_SST39SF040(unsigned long base_address) {
  // TODO
}

int get_SST39SF040_id() {
  // TODO
}

void set_AT28C64_address(int address, boolean is_output_enabled) {
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, (address >> 8) | (is_output_enabled ? 0 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

void write_to_AT28C64(int address, byte data) {
  byte data_read = read_from_AT28C64(address);
  if (data_read != data)
  {
    set_data_pins_mode(OUTPUT);
    set_AT28C64_address(address, WRITE);

    byte curr_data = data;
    for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
    {
      digitalWrite(pin, curr_data & 0x80);
      curr_data <<= 1;
    }

    digitalWrite(WRITE_ENABLE, LOW);
    delayMicroseconds(1);
    digitalWrite(WRITE_ENABLE, HIGH);

    // error correction
    byte l = read_from_AT28C64(address);
    int cnt = 1000;
    while ((cnt > 0) && (l != data))
    {
      cnt--;
      if ((address & 0xff) == 0)
        l = read_from_AT28C64(address + 1);
      else
        l = read_from_AT28C64(address - 1);
      l = read_from_AT28C64(address);
    }
    if (l != data)
    {
      char buf[60];
      sprintf(buf, "\r\nWRITE ERROR on %04x! (%02x %02x)\r\n", address, data, l);
      Serial.println(buf);
      num_write_errors++;
    }
  }
}

byte read_from_AT28C64(int address) {
  set_data_pins_mode(INPUT);

  set_AT28C64_address(address, READ);

  byte data_read = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
  {
    data_read = data_read * 2 + digitalRead(pin);
  }
  return data_read;
}

void print_256_bytes_from_AT28C64(int base_address) { // prints the next 256 bytes of data starting from the 'base_address'
  for (int base = base_address; base < base_address + 256; base += 16)
  {
    byte data[16];

    for (int offset = 0; offset < 16; offset++)
    {
      data[offset] = read_from_AT28C64(base + offset);
    }

    char buf[80];
    sprintf(buf, "%04x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8],
            data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);
  }
}

uint32_t _goto(int label_number) { // returns the formatted control word for a predefined label
  /*
  given a 'label_number', an 8-bit value, the value is fed into the label ROM via the data bus output MUX (4 bits) and ALU function (4 bits) control lines, which consequently outputs a predefined label into the mcirocode ROM
  */
  uint32_t predefined_label = (((uint32_t)label_number) << 24); // shift the bits into the correct control line places
  return _CIL | predefined_label;
}

/* defining core functions */

void write_microcode_byte(int address, uint32_t code) { // writes the necessary control line outputs to a microcode ROM given the the address and active control word signals
  if (!IS_MCODE_ROM)
    return; // makes sure we only write to the microcode ROMs

  uint32_t inverted = code ^ _negatives;               // inverts all of the active low signals in the control word
  byte data = (inverted >> (CURR_ROM_NUM * 8)) & 0xFF; // shifts the inverted word to the desired place

  write_to_AT28C64(address, data);
}

void write_label_byte(int relevant_label_flags, boolean is_an_opcode, int curr_label_number, int destination_address) {
  /*
  writes a byte of the 12-bit destination address at a label address based on the parameters
  - relevant_label_flags : a bitfield encoding the current flag combo we are dealing with
  - is_an_opcode         : true/false if this label represents an instruction opcode or a predefined label
  - curr_label_number    : either the instruction opcode of a predefined label
  - destination_address  : the 12-bit target microcode address to jump to
  */
  if (!IS_LABEL_ROM)
    return;
  
  unsigned long full_label_address = curr_label_number + (is_an_opcode ? 0 : 512) + ((unsigned long)relevant_label_flags) * 1024;
  /*
  A0-A7  : curr_label_address
  A8     : 9th bit
  A9     : is_jump_to_RBE_taken
  A10-13 : relevant_label_flags
  */
  unsigned long full_label_address_with_9th_bit_set = curr_label_number + (256) + (is_an_opcode ? 0 : 512) + ((unsigned long)relevant_label_flags) * 1024;

  byte data = (destination_address >> ((CURR_ROM_NUM - FIRST_LABEL_ROM) * 8)) & 0xFF;

  write_data_to_SST39SF040(full_label_address, data);
  write_data_to_SST39SF040(full_label_address_with_9th_bit_set, data); // REMOVE ONCE YOU FIGURE OUT HOW TO DO 9TH BIT OPCODES
}

void write_label_if_flags_set(int relevant_label_flags, boolean is_an_opcode, int curr_label_number, int destination_address) {
  if (!IS_LABEL_ROM) return;

  int BASE_CPU_FLAG_COMBO = _F_C | _F_Z | _F_N | _F_V | _F_II;                                            // (exclude IF, IR, and IC flags)
  for (int curr_flag_combo = 0; curr_flag_combo < BASE_CPU_FLAG_COMBO; curr_flag_combo++) {                             // we loop over all combos of thes flags
    int active_flags = curr_flag_combo & relevant_label_flags;
    if (active_flags != 0) {
      write_label_byte(curr_flag_combo | _F_IR, is_an_opcode, curr_label_number, destination_address);    // we go to the destination if IF is inactive
      if (is_an_opcode) {                                                                                 // case 1: handle flags if we are dealing with an opcode
        write_label_byte(curr_flag_combo | _F_IF, true, curr_label_number, fetch_address);                // we go to fetch address if we are dealing with an opcode and instruction request is inactive
        if (curr_flag_combo & _F_II == 0) {                                                               // handles interrupts: checks if interrupt inhibit is clear
          write_label_byte(curr_flag_combo | _F_IR | _F_IF, true, curr_label_number, interrupt_address);  // if interrupt inhibit is clear, we can proceed to interrupt sequence
        } else {
          write_label_byte(curr_flag_combo | _F_IR | _F_IF, true, curr_label_number, fetch_address);      // if interrupt inhibit is set, we cannot proceed to interrupt sequence
        }
      } else {                                                                                            // case 2: handle flags if we are dealing with a predefined label
        write_label_byte(curr_flag_combo | _F_IF        , false, curr_label_number, destination_address); // predefined label just jumps to destination in microcode
        write_label_byte(curr_flag_combo | _F_IR | _F_IF, false, curr_label_number, destination_address); // predefined label just jumps to destination in microcode
      }
    } else {
      continue; // some addresses spaces left empty which may be filled up later
    }
  }
}

void write_label_if_flags_clear(int relevant_label_flags, boolean is_an_opcode, int curr_label_number, int destination_address) { // basically same as 'write_label_if_flags_set' but for 'not' logic (ex: jump if not set)
  if (!IS_LABEL_ROM) return;

  int BASE_CPU_FLAG_COMBO = _F_C | _F_Z | _F_N | _F_V | _F_II;                                            // (exclude IF, IR, and IC flags)
  for (int curr_flag_combo = 0; curr_flag_combo < BASE_CPU_FLAG_COMBO; curr_flag_combo++) {                             // we loop over all combos of thes flags
    int active_flags = curr_flag_combo & relevant_label_flags;
    if (active_flags == 0) {
      write_label_byte(curr_flag_combo | _F_IR, is_an_opcode, curr_label_number, destination_address);    // we go to the destination if IF is inactive
      if (is_an_opcode) {                                                                                 // case 1: handle flags if we are dealing with an opcode
        write_label_byte(curr_flag_combo | _F_IF, true, curr_label_number, fetch_address);                // we go to fetch address if we are dealing with an opcode and instruction request is inactive
        if (curr_flag_combo & _F_II == 0) {                                                               // handles interrupts: checks if interrupt inhibit is clear
          write_label_byte(curr_flag_combo | _F_IR | _F_IF, true, curr_label_number, interrupt_address);  // if interrupt inhibit is clear, we can proceed to interrupt sequence
        } else {
          write_label_byte(curr_flag_combo | _F_IR | _F_IF, true, curr_label_number, fetch_address);      // if interrupt inhibit is set, we cannot proceed to interrupt sequence
        }
      } else {                                                                                            // case 2: handle flags if we are dealing with a predefined label
        write_label_byte(curr_flag_combo | _F_IF        , false, curr_label_number, destination_address); // predefined label just jumps to destination in microcode
        write_label_byte(curr_flag_combo | _F_IR | _F_IF, false, curr_label_number, destination_address); // predefined label just jumps to destination in microcode
      }
    } else {
      continue; // some addresses spaces left empty which may be filled up later
    }
  }
}

void write_label_unconditional(boolean is_an_opcode, int curr_label_number, int destination_address) {
  int ALL_FLAGS_INACTIVE = 0;
  write_label_if_flags_clear(ALL_FLAGS_INACTIVE, is_an_opcode, curr_label_number, destination_address);
}

void print_curr_instruction(char* curr_instruction_name, int curr_instruction_opcode) {
  char buffer[60];
  if        (strstr(curr_instruction_name, ",#") != NULL) {                           // immediate data (1 byte) operand
    sprintf(buffer, "%s{val}\t=> 0x%02x @ val[7:0]", curr_instruction_name, curr_instruction_opcode);
  } else if (strstr(curr_instruction_name, "address") != NULL) {                      // address (2 bytes) operand
    sprintf(buffer, "%s\t=> 0x%02x @ val[7:0] @ val[15:8]", curr_instruction_name, curr_instruction_opcode);
    char* s = strstr(buffer, "address");                                              // reformatting output string
    s[0] = ' ';
    s[1] = '{';
    s[2] = 'a';
    s[3] = 'd';
    s[4] = 'd';
    s[5] = 'r';
    s[6] = '}';
  } else if (strstr(curr_instruction_name, "value") != NULL) {                        // indirect data operand
    sprintf(buffer, " %s\t=> 0x%02x @ v[7:0]", curr_instruction_name, curr_instruction_opcode);
    char* s = strstr(buffer, "value");
    s[0] = '{';
    s[1] = 'v';
    s[2] = 'a';
    s[3] = 'l';
    s[4] = '}';
  } else {                                                                            // no operand
    sprintf(buffer, "%s\t=> 0x%02x", curr_instruction_name, curr_instruction_opcode);
  }
  Serial.println(buffer);
}

void setup()
{
  /* initialization steps */
  Serial.begin(57600);

  char buffer[80];
  sprintf(buffer, "Begin writing to ROM #%d", CURR_ROM_NUM);
  Serial.println(buffer);

  init_ports();
  unsigned long start_time = millis();

  /*
  BEHOLD THE HOLY MICROCODE
  */

  int curr_mc_address  = 0;
  int curr_instruction_opcode = 0;

  /* reset logic code */
  write_microcode_byte(curr_mc_address++, 0x00); // noop (this is the reset entry point)

  // ensure interrupt inhibited: basically we jump to an address to toggle _II if it is set, and we jump past that address if _II is not set
  int predefined_label_0 = 0;
  write_microcode_byte(curr_mc_address++, _goto(predefined_label_0));                          // we jump to predefined label 0
  write_label_if_flags_set(_F_II, IS_PREDEFINED_LABEL, predefined_label_0, curr_mc_address);   // define the next line as the address for the entry point to predefined label 0 if interrupt inhibit is set

  write_microcode_byte(curr_mc_address++, _CTI);                                               // we toggle the interrupt so that it is clear
  write_label_if_flags_clear(_F_II, IS_PREDEFINED_LABEL, predefined_label_0, curr_mc_address); // define the next line as the address for the entry point to predefined label 0 if interrupt inhibit is clear (we don't have to clear interrupt inhibit as it is already clear)

  // initializes program counter from 0xFFFC in memory(reset vector address hardcoded in ROM)
   /*TODO: write this in ROM*/
  write_microcode_byte(curr_mc_address++, _ALU_xFF | _REW);               // write 0xFF to E reg.
  write_microcode_byte(curr_mc_address++, _FLG_CLC | _RFW);               // clear carry for shift
  write_microcode_byte(curr_mc_address++, _REE | _TUW | _ALU_LSL | _REW); // we write 0xFF to transfer reg's high byte, and then left shift and write E reg. so it contains 0xFE
  write_microcode_byte(curr_mc_address++, _REE | _ALU_LSL | _REW);        // we shift E reg. so it contains 0xFC
  write_microcode_byte(curr_mc_address++, _REE | _TLW);                   // write 0xFC to transfer reg's low byte
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _RCW);            // we address into memory 0xFFFC and output data byte into C reg.

  write_microcode_byte(curr_mc_address++, _ALU_xFF | _REW);               // write 0xFF to E reg.
  write_microcode_byte(curr_mc_address++, _FLG_CLC | _RFW);               // clear carry for shift
  write_microcode_byte(curr_mc_address++, _REE | _ALU_LSL | _REW | _RFW); // we left shift and write E reg. so it contains 0xFE, enable flag so carry is set
  write_microcode_byte(curr_mc_address++, _REE | _ALU_LSL | _REW);        // we left shift and write E reg. so it contains 0xFD (carry flag from previous line shifts into the byte)
  write_microcode_byte(curr_mc_address++, _REE | _TRL);                   // write 0xFD to transfer register's low byte
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _RDW);            // we address into memory 0xFFFD and output data byte into D reg.

  write_microcode_byte(curr_mc_address++, _RDE | _TRU); // write D reg. to transfer register's high byte
  write_microcode_byte(curr_mc_address++, _RCE | _TRL); // write C reg. to transfer register's low  byte 
  write_microcode_byte(curr_mc_address++, _TRE | _PCW); // write transfer register into program counter

  // initialize registers to zero
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);                                       // write x00   to E reg.
  write_microcode_byte(curr_mc_address++, _REE | _TLW | _TUW | _RAW | _RBW | _RCW | _RDW | _OTW); // write x0000 to transfer reg. and x00 to regs. A, B, C, D, and O
  write_microcode_byte(curr_mc_address++, _SPW | _TRE);                                           // write x0000 to stack pointer
  write_microcode_byte(curr_mc_address++, _SPD | _SPC);                                           // count down once so stack pointer contains xFFFF
  write_microcode_byte(curr_mc_address++, _REE | _FLG_BUS | _RFW | _CIC);                         // clears flags register to x0
  // now we fall to fetch code (_CIC is set)

  /* fetch code */
  fetch_address = curr_mc_address;
  write_microcode_byte(curr_mc_address++, _PCE | _PCC | _MME | _CIC | _CIL);

  /* interrupt routine code */
  /* 
  1) Push Flag Registers onto Stack
  2) Push Program Counter onto Stack
  3) Set Interrupt Inhibit Flag
  4) Load Program Counter with address from 0xFFFE / 0xFFFF
  */
  interrupt_address = curr_mc_address;
   /*TODO: understand interrupt code*/



  /*||===========================||*/
  /*|| simple unconditional shit ||*/
  /*||===========================||*/

  // NOP - No Operation
  int nop_address = curr_mc_address;
  print_curr_instruction("NOP", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _CIC );

  // HLT - Halt
  print_curr_instruction("HLT", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _HLT | _CIC );

  // CLC - Clear Carry
  print_curr_instruction("CLC", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _FLG_CLC | _RFW | _CIC );

  // STC - Set Carry
  print_curr_instruction("STC", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _FLG_STC | _RFW | _CIC );

  int toggle_interrupt_inhibit_address = curr_mc_address;
  write_microcode_byte(curr_mc_address++, _CTI | _CIC);

  // SII - Set Interrupt Inhibit: disables interrupts
  print_curr_instruction("SII", curr_instruction_opcode);
  write_label_if_flags_set(_F_II,   IS_OPCODE, curr_instruction_opcode, nop_address);
  write_label_if_flags_clear(_F_II, IS_OPCODE, curr_instruction_opcode++, toggle_interrupt_inhibit_address);

  // CII - Clear Interrupt Inhibit: enables interrupts
  print_curr_instruction("CII", curr_instruction_opcode);
  write_label_if_flags_set(_F_II,   IS_OPCODE, curr_instruction_opcode, toggle_interrupt_inhibit_address);
  write_label_if_flags_clear(_F_II, IS_OPCODE, curr_instruction_opcode++, nop_address);

  // RTS - Return from Subroutine
  print_curr_instruction("RTS", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _SPC);               
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _TUW); // write top byte
  write_microcode_byte(curr_mc_address++, _SPC);
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _TLW); // write low byte
  write_microcode_byte(curr_mc_address++, _TRE | _PCW | _CIC); // transfer reg. to program counter

  // RTI - Return from Interrupt
  /*
  1) Pull Program Counter from Stack
  2) Pull Flags Register from Stack
  3) Clear Interrupt Inhibit Flag
  */
  print_curr_instruction("RTI", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _SPC);               
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _TUW); // write top byte
  write_microcode_byte(curr_mc_address++, _SPC);
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _TLW); // write low byte
  write_microcode_byte(curr_mc_address++, _TRE | _PCW | _SPC); // transfer reg. to program counter
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _FLG_BUS | _RFW | _CTI | _CIC);

  
  /*||==============================||*/
  /*|| 2 operand unconditional shit ||*/
  /*|| (MOV,ADD,SUB,AND,OR,XOR,CMP) ||*/
  /*||==============================||*/

  int dest = 0; // destination index into the 'register arrays'
  int src  = 0; // source index into the 'register arrays'

  // register addressing mode: retrieve the data from a specified register (4 * 3 * 7 = 84 instructions)
  while (dest < 4) {
    src = 0; // reset the source index
    while (src < 4) {
      if (src != dest) {
        // MOV: src -> dest
        sprintf(buffer, "MOV R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]); // ex: MOV RA,RB
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src] | GP_REG_WRITE[dest] | _CIC);

        // ADD: dest + src -> dest
        sprintf(buffer, "ADD R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]);
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src]  | _ALU_ADD | _REW | _RFW);
        write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE     | _CIC);

        // SUB: dest - src -> dest
        sprintf(buffer, "SUB R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]);
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src]  | _ALU_SUB | _REW | _RFW);
        write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE     | _CIC);

        // AND: dest & src -> dest
        sprintf(buffer, "AND R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]);
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src]  | _ALU_AND | _REW | _RFW);
        write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE     | _CIC);

        // OR: dest | src -> dest
        sprintf(buffer, "OR R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]);
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src]  | _ALU_OR  | _REW | _RFW);
        write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE     | _CIC);

        // XOR: dest ^ src -> dest
        sprintf(buffer, "XOR R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]);
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src]  | _ALU_XOR  | _REW | _RFW);
        write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE     | _CIC);

        // CMP: dest > src -> F reg. = XX00 ; dest < src -> F reg. = XX10 ; dest = src -> F reg. = XX01 
        sprintf(buffer, "CMP R%c,R%c", GP_REGISTERS[dest], GP_REGISTERS[src]);
        print_curr_instruction(buffer, curr_instruction_opcode);
        write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
        write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[src]  | _ALU_CMP | _RFW | _CIC);
      }
      src++;
    }
    dest++;
  }

  // immediate addressing mode: retrieve the data right after the opcode in memory (4 * 7 = 28 instructions)
  for (int dest = 0; dest < 4; dest++) {
    // MOV: imm8 -> dest
    sprintf(buffer, "MOV R%c,#", GP_REGISTERS[dest]); // ex: MOV RA,0xFF
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | GP_REG_WRITE[dest] | _PCC | _CIC); // NOTE: increment PC since we already incremented in fetch cycle
    
    // ADD: dest + imm8 -> dest
    sprintf(buffer, "ADD R%c,#", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _ALU_ADD | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);

    // SUB: dest - imm8 -> dest
    sprintf(buffer, "SUB R%c,#", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _ALU_SUB | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);

    // AND: dest & imm8 -> dest
    sprintf(buffer, "AND R%c,#", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _ALU_AND | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);

    // OR: dest | imm8 -> dest
    sprintf(buffer, "OR R%c,#", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _ALU_OR | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);

    // XOR: dest ^ imm8 -> dest
    sprintf(buffer, "XOR R%c,#", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _ALU_XOR | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);

    // CMP: dest > imm8 -> F reg. = XX00 ; dest < imm8 -> F reg. = XX10 ; dest = imm8 -> F reg. = XX01
    sprintf(buffer, "CMP R%c,#", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _ALU_CMP | _RFW | _PCC | _CIC);
  }

  // absolute addressing mode: retrieve the data from a location in memory, address from the two bytes after the opcode (4 * 7 = 28 instructions)
  for (int dest = 0; dest < 4; dest++) {
    // MOV: [imm16] -> dest
    sprintf(buffer, "MOV R%c,address", GP_REGISTERS[dest]); // ex: MOV RA,0x1234
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | GP_REG_WRITE[dest] | _PCC | _CIC); // NOTE: need to increment PC

    // MOV: dest -> [imm16]
    sprintf(buffer, "MOV address,R%c", GP_REGISTERS[dest]); // ex: MOV 0x1234,RA
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, _TRE | _MMW | GP_REG_ENBLE[dest] | _PCC | _CIC);
    
    // ADD: dest + [imm16] -> dest
    sprintf(buffer, "ADD R%c,address", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_ADD | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);
    
    // SUB: dest - [imm16] -> dest
    sprintf(buffer, "SUB R%c,address", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_SUB | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);
    
    // AND: dest & [imm16] -> dest
    sprintf(buffer, "AND R%c,address", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_AND | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);
    
    // OR: dest | [imm16] -> dest
    sprintf(buffer, "OR R%c,address", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_OR | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);
    
    // XOR: dest ^ [imm16] -> dest
    sprintf(buffer, "XOR R%c,address", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_XOR | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _PCC | _CIC);
    
    // CMP: dest > [imm16] -> F reg. = XX00 ; dest < [imm16] -> F reg. = XX10 ; dest = [imm16] -> F reg. = XX01
    sprintf(buffer, "CMP R%c,address", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
    write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_CMP | _RFW | _PCC | _CIC);
  }

  // indirect addressing mode: retrieve the data from a location in memory, address from the C reg. (low byte) and D reg. (high byte)
  for (int dest = 0; dest < 4; dest++) {
    // MOV: [RCD] -> dest
    sprintf(buffer, "MOV R%c,[RCD]", GP_REGISTERS[dest]); // ex: MOV RA,[RCD]
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, _TRE | _MME | GP_REG_WRITE[dest] | _CIC);

    // MOV: dest -> [RCD]
    sprintf(buffer, "MOV [RCD]R%c", GP_REGISTERS[dest]); // ex: MOV [RCD],RA
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, _TRE | _MMW | GP_REG_ENBLE[dest] | _CIC);
    
    // ADD: dest + [RCD] -> dest
    sprintf(buffer, "ADD R%c,[RCD]", GP_REGISTERS[dest]); // ex: ADD RA,[RCD]
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_ADD | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _CIC);

    // SUB: dest - [RCD] -> dest
    sprintf(buffer, "SUB R%c,[RCD]", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_SUB | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _CIC);

    // AND: dest & [RCD] -> dest
    sprintf(buffer, "AND R%c,[RCD]", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_AND | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _CIC);
    
    // OR: dest | [RCD] -> dest
    sprintf(buffer, "OR R%c,[RCD]", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_OR | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _CIC);
        
    // XOR: dest ^ [RCD] -> dest
    sprintf(buffer, "XOR R%c,[RCD]", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_XOR | _REW | _RFW);
    write_microcode_byte(curr_mc_address++, _REE | GP_REG_WRITE[dest] | _CIC);
    
    // CMP: dest > [RCD] -> F reg. = XX00 ; dest < [RCD] -> F reg. = XX10 ; dest = [RCD] -> F reg. = XX01
    sprintf(buffer, "CMP R%c,[RCD]", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _RCE | _TLW); // C reg. contains low  byte
    write_microcode_byte(curr_mc_address++, _RDE | _TUW); // D reg. contains high byte
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_BUS | _REW);
    write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_CMP | _RFW | _CIC);
  }

  /*||==============================||*/
  /*|| 1 operand unconditional shit ||*/
  /*||==============================||*/


  /* OUT: output onto output register/display */

  // OUT register addressed: dest -> O reg.
  for (int dest = 0; dest < 4; dest++) {
    sprintf(buffer, "OUT R%c", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _OTW | _CIC);
  }

  // OUT absolutely addressed: [imm16] -> O reg.
  print_curr_instruction("OUT address", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC); // we don't have to increment PC before since it is incremented at end of fetch code
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _OTW | _PCC | _CIC);

  // OUT indirectly addressed (low byte in C reg. high byte in D reg.): [RCD] -> O reg.
  print_curr_instruction("OUT [RCD]", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RCE | _TLW );
  write_microcode_byte(curr_mc_address++, _RDE | _TUW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _OTW | _CIC);


  /* LSL: Logical Shift Left */

  // LSL register addressed: dest << 1 -> dest
  for (int dest = 0; dest < 4; dest++) {
    sprintf(buffer, "LSL R%c", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_LSL | _REW | _RFW); 
    write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE | _CIC);
  }

  // LSL absolutely addressed: [imm16] << 1 -> [imm16]
  print_curr_instruction("LSL address", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC); // we don't have to increment PC before since it is incremented at end of fetch code
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_LSL | _REW | _RFW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MMW | _REE     | _PCC | _CIC);

  // LSL indirectly addressed (low byte in C reg. high byte in D reg.): [RCD] << 1 -> [RCD]
  print_curr_instruction("LSL [RCD]", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RCE | _TLW);
  write_microcode_byte(curr_mc_address++, _RDE | _TUW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_LSL | _REW | _RFW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MMW | _REE     | _CIC);


  /* LSR: Logical Shift Right */

  // LSL register addressed: dest >> 1 -> dest
  for (int dest = 0; dest < 4; dest++) {
    sprintf(buffer, "LSR R%c", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _ALU_LSR | _REW | _RFW); 
    write_microcode_byte(curr_mc_address++, GP_REG_WRITE[dest] | _REE | _CIC);
  }

  // LSL absolutely addressed: [imm16] >> 1 -> [imm16]
  print_curr_instruction("LSR address", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC); // we don't have to increment PC before since it is incremented at end of fetch code
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_LSR | _REW | _RFW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MMW | _REE     | _PCC | _CIC);

  // LSL indirectly addressed (low byte in C reg. high byte in D reg.): [RCD] >> 1 -> [RCD]
  print_curr_instruction("LSR [RCD]", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RCE | _TLW);
  write_microcode_byte(curr_mc_address++, _RDE | _TUW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_LSR | _REW | _RFW); 
  write_microcode_byte(curr_mc_address++, _TRE | _MMW | _REE     | _CIC);
  

  /* PSH/POP: Push and Pop from stack pointer */
  for (int dest = 0; dest < 4; dest++) {
    // PSH: dest -> [SP--]
    sprintf(buffer, "PSH R%c", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, GP_REG_ENBLE[dest] | _SPE | _MMW | _SPD); // dest -> [SP]
    write_microcode_byte(curr_mc_address++, _SPC | _SPD | _CIC);                      // decrement SP ([SP] now useable)

    // POP: [++SP] -> dest
    sprintf(buffer, "POP R%c", GP_REGISTERS[dest]);
    print_curr_instruction(buffer, curr_instruction_opcode);
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
    write_microcode_byte(curr_mc_address++, _SPC);                                    // increment SP
    write_microcode_byte(curr_mc_address++, _SPE | _MME | GP_REG_WRITE[dest] | _CIC); // [SP] -> dest ([SP] now useable)
  }

  // PSF: Push Flags onto Stack
  print_curr_instruction("PSF", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RFE | _SPE | _MMW | _SPD); // F reg. -> [SP]
  write_microcode_byte(curr_mc_address++, _SPC | _SPD | _CIC);        // decrement SP

  // PPF: Pop Flags off of Stack
  print_curr_instruction("PPF", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _SPC | _ALU_x00 | _REW);       // increment SP, x00 -> E reg.
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _ALU_OR | _RFW); // flags of [SP++] | x00 go into F reg.

  int predefined_label_1 = 1;                                         // need to prevent messing up pre-subroutine state of interrupt flags
  write_microcode_byte(curr_mc_address++, _goto(predefined_label_1)); // based on N and II flags, we toggle interrupt
  
  int toggle_interrupt_address = curr_mc_address;
  write_microcode_byte(curr_mc_address++, _CTI);

  int no_toggle_interrupt_address = curr_mc_address;
  if (IS_LABEL_ROM) {
    int ALL_FLAGS = _F_C | _F_Z | _F_N | _F_V | _F_II | _F_IR | _F_IF;
    for (int curr_flag_combo = 0; curr_flag_combo <= ALL_FLAGS; curr_flag_combo++) {
      boolean IS_NEGATIVE_AND_INTERRUPT_INHIBIT_CLEAR = (curr_flag_combo & (_F_N | _F_II)) == 0;
      if (IS_NEGATIVE_AND_INTERRUPT_INHIBIT_CLEAR) {                                                                            // checks if neither the negative flag nor interrupt inhibit flag is set
        write_label_byte(curr_flag_combo               , IS_PREDEFINED_LABEL, predefined_label_1, no_toggle_interrupt_address); // if both  clear, don't toggle interrupt -> II now clear
        write_label_byte(curr_flag_combo | _F_N        , IS_PREDEFINED_LABEL, predefined_label_1, toggle_interrupt_address);    // if only N  set, toggle interrupt       -> II now set
        write_label_byte(curr_flag_combo | _F_II       , IS_PREDEFINED_LABEL, predefined_label_1, toggle_interrupt_address);    // if only II set, toggle interrupt       -> II now clear
        write_label_byte(curr_flag_combo | _F_N | _F_II, IS_PREDEFINED_LABEL, predefined_label_1, no_toggle_interrupt_address); // if both    set, don't toggle interrupt -> II now set
      }
    }
  }
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _FLG_BUS | _RFW | _CIC); // pop flags from stack and write into F reg.


  /* Stack Pointer MOV */

  // MOV SP,RCD: C reg., D reg. -> SP
  print_curr_instruction("MOV SP,RCD", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RCE | _TLW);
  write_microcode_byte(curr_mc_address++, _RDE | _TUW);
  write_microcode_byte(curr_mc_address++, _TRE | _SPW | _CIC);

  // MOV RCD,SP: SP -> C reg., D reg.
  print_curr_instruction("MOV RCD,SP", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _SPL | _RCW);
  write_microcode_byte(curr_mc_address++, _SPU | _RDW | _CIC);


  
  /*||===================||*/
  /*|| jump instructions ||*/
  /*||===================||*/

  /* immediate addressing mode */

  int jump_address = curr_mc_address; // [PC++][PC] -> [PC] (puts imm16 into SP)
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC); // currently on lower byte of imm16
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW);
  write_microcode_byte(curr_mc_address++, _TRE | _PCW | _CIC);        // load imm16 into PC, go to fetch

  int no_jump_address = curr_mc_address;
  write_microcode_byte(curr_mc_address++, _PCC);        // skip first  half of imm16
  write_microcode_byte(curr_mc_address++, _PCC | _CIC); // skip second half of imm16, go to fetch
  
  // JMP: unconditional jump
  print_curr_instruction("JMP address", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, jump_address);

  // JSR address : jump to subroutine
  print_curr_instruction("JSR address", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW | _PCC);
  write_microcode_byte(curr_mc_address++, _PCL | _SPE | _MMW | _SPD);        // first push low  byte onto stack
  write_microcode_byte(curr_mc_address++, _SPC | _SPD);
  write_microcode_byte(curr_mc_address++, _PCU | _SPE | _MMW | _SPD);        // then  push high byte onto stack
  write_microcode_byte(curr_mc_address++, _TRE | _PCW | _SPC | _SPD | _CIC); // imm16 -> PC, decrement SP

  // JCS address : jump if carry set
  print_curr_instruction("JCS address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_C, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_C, IS_OPCODE, curr_instruction_opcode++, no_jump_address);

  // JZS address : jump if zero set
  print_curr_instruction("JZS address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_Z, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_Z, IS_OPCODE, curr_instruction_opcode++, no_jump_address);

  // JNS address : jump if negative set
  print_curr_instruction("JNS address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_N, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_N, IS_OPCODE, curr_instruction_opcode++, no_jump_address);

  // JVS address : jump if overflow set
  print_curr_instruction("JVS address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_V, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_V, IS_OPCODE, curr_instruction_opcode++, no_jump_address);


  // JNC address : jump if carry not set
  print_curr_instruction("JNC address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_C, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_C, IS_OPCODE, curr_instruction_opcode++,    jump_address);

  // JNZ address : jump if zero not set
  print_curr_instruction("JNZ address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_Z, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_Z, IS_OPCODE, curr_instruction_opcode++,    jump_address);

  // JNN address : jump if negative not set
  print_curr_instruction("JNN address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_N, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_N, IS_OPCODE, curr_instruction_opcode++,    jump_address);

  // JNV address : jump if overflow not set
  print_curr_instruction("JNV address", curr_instruction_opcode);
  write_label_if_flags_set  (_F_V, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_V, IS_OPCODE, curr_instruction_opcode++,    jump_address);


  /* indirect addressing mode */

  jump_address = curr_mc_address;
  write_microcode_byte(curr_mc_address++, _RCE | _TLW);
  write_microcode_byte(curr_mc_address++, _RDE | _TUW);
  write_microcode_byte(curr_mc_address++, _TRE | _PCW | _CIC);

  no_jump_address = nop_address; // we go to noop since we do nothing (no imm16), duh

  // JMP [RCD] : unconditional jump
  print_curr_instruction("JMP [RCD]", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, jump_address);

  // JSR [RCD] : jump to subroutine
  print_curr_instruction("JSR [RCD]", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RCE | _TLW);
  write_microcode_byte(curr_mc_address++, _RDE | _TUW);
  write_microcode_byte(curr_mc_address++, _PCL | _SPE | _MMW | _SPD);        // first push low  byte onto stack
  write_microcode_byte(curr_mc_address++, _SPC | _SPD);
  write_microcode_byte(curr_mc_address++, _PCU | _SPE | _MMW | _SPD);        // then  push high byte onto stack
  write_microcode_byte(curr_mc_address++, _TRE | _PCW | _SPC | _SPD | _CIC); // imm16 -> PC, decrement SP

  // JCS [RCD] : jump if carry set
  print_curr_instruction("JCS [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_C, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_C, IS_OPCODE, curr_instruction_opcode++, no_jump_address);

  // JZS [RCD] : jump if zero set
  print_curr_instruction("JZS [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_Z, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_Z, IS_OPCODE, curr_instruction_opcode++, no_jump_address);

  // JNS [RCD] : jump if negative set
  print_curr_instruction("JNS [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_N, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_N, IS_OPCODE, curr_instruction_opcode++, no_jump_address);

  // JVS [RCD] : jump if overflow set
  print_curr_instruction("JVS [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_V, IS_OPCODE, curr_instruction_opcode,      jump_address);
  write_label_if_flags_clear(_F_V, IS_OPCODE, curr_instruction_opcode++, no_jump_address);


  // JNC [RCD] : jump if carry not set
  print_curr_instruction("JNC [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_C, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_C, IS_OPCODE, curr_instruction_opcode++,    jump_address);

  // JNZ [RCD] : jump if zero not set
  print_curr_instruction("JNZ [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_Z, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_Z, IS_OPCODE, curr_instruction_opcode++,    jump_address);

  // JNN [RCD] : jump if negative not set
  print_curr_instruction("JNN [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_N, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_N, IS_OPCODE, curr_instruction_opcode++,    jump_address);

  // JNV [RCD] : jump if overflow not set
  print_curr_instruction("JNV [RCD]", curr_instruction_opcode);
  write_label_if_flags_set  (_F_V, IS_OPCODE, curr_instruction_opcode,   no_jump_address);
  write_label_if_flags_clear(_F_V, IS_OPCODE, curr_instruction_opcode++,    jump_address);


  /*||========================||*/
  /*|| basic I/O instructions ||*/
  /*||      (port shit)       ||*/
  /*||========================||*/

  // OUT value,RA : RA -> PORT[imm8]
  print_curr_instruction("OUT value,RA", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _PSS | _PCC); // imm8 -> Port Select
  write_microcode_byte(curr_mc_address++, _RAE | _PSW | _CIC);

  // INP RA,value : PORT[imm8] -> RA
  print_curr_instruction("INP RA,value", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _PSS | _PCC); // imm8 -> Port Select
  write_microcode_byte(curr_mc_address++, _PSE | _RAW | _CIC);

  // OUT RB,RA : RA -> PORT[RB]
  print_curr_instruction("OUT RB,RA", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RBE | _PSS);        // B reg. -> Port Select
  write_microcode_byte(curr_mc_address++, _RAE | _PSW | _CIC); // A reg. -> Port

  // INP RA,RB : PORT[RB] -> RA
  print_curr_instruction("INP RA,RB", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RBE | _PSS);        // B reg. -> Port Select
  write_microcode_byte(curr_mc_address++, _PSE | _RAW | _CIC); // A reg. -> Port


  /*||=========================================||*/
  /*|| MOV: absolute indexed, indirect indexed ||*/
  /*||           (indexed addressing)          ||*/
  /*||=========================================||*/

  // MOV RA,address,RB : [imm16 + RB] -> RA
  print_curr_instruction("MOV RA,address,RB", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RFE     | _SPE | _MMW);                   // push flags onto stack
  write_microcode_byte(curr_mc_address++, _FLG_CLC | _RFW);                          // clear carry for adding B reg.
  write_microcode_byte(curr_mc_address++, _PCE     | _MME | _ALU_BUS | _REW);        // imm16 lower byte -> E reg.
  write_microcode_byte(curr_mc_address++, _ALU_ADD | _REW | _RBE     | _RFW | _PCC); // E reg. + B reg. -> E reg., increment PC
  write_microcode_byte(curr_mc_address++, _TLW     | _REE);                          // E reg. -> lower TR
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);                          // x00 -> E reg.
  write_microcode_byte(curr_mc_address++, _PCE     | _MME | _ALU_ADD | _REW);        // imm16 upper byte + carry? -> E reg.
  write_microcode_byte(curr_mc_address++, _TUW     | _REE | _PCC);                   // E reg. -> upper TR, increment PC
  write_microcode_byte(curr_mc_address++, _TRE     | _MME | _RAW);                   // [TR] -> A reg.
  write_microcode_byte(curr_mc_address++, _SPE     | _MME | _FLG_BUS | _RFW | _CIC); // restore flags

  // MOV RA,[address],RB : [[imm16] + RB] -> RA
  print_curr_instruction("MOV RA,[address],RB", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RFE | _SPE | _MMW | _SPD);               // RF -> [SP]              (push flags onto stack)
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC | _SPD | _SPC); // [PC] -> TRL, PC++, SP-- (imm16 lower byte -> TR lower, increment PC, decrement SP)
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW | _PCC);               // [PC] -> TRU, PC++       (imm16 upper byte -> TR upper, increment PC)
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_BUS | _REW);           // [TR] -> RE              ([imm16] -> E reg.)
  write_microcode_byte(curr_mc_address++, _REE | _SPE | _MMW | _FLG_STC | _RFW);    // RE -> [SP], RF=XXX1     (push [imm16] onto stack (will use later), set carry to get [imm16+1])
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);                         // RE = x00                (set E reg. to zero)
  write_microcode_byte(curr_mc_address++, _TRL | _ALU_ADD | _REW | _RFW);           // RE += TRL, RF=XXXX      
  write_microcode_byte(curr_mc_address++, _REE | _TLW);                             // RE -> TRL
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);                         // RE = x00
  write_microcode_byte(curr_mc_address++, _TRU | _ALU_ADD | _REW | _RFW);           // RE += TRU, RF=XXXX
  write_microcode_byte(curr_mc_address++, _TUW | _REE);                             // RE -> TRU               (TR now contains imm16+1)
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_BUS | _REW);           // [TR] -> RE              (E reg. = [imm16+1], E reg. is a temporary step cuz we modify TR in next step)
  write_microcode_byte(curr_mc_address++, _REE | _TUW);                             // RE -> TRU               
  write_microcode_byte(curr_mc_address++, _RBE | _ALU_BUS | _REW);                  // RB -> RE
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _ALU_ADD | _REW | _RFW);    // RE += [SP], RF=XXXX     (add B reg. to [imm16])
  write_microcode_byte(curr_mc_address++, _REE | _TLW | _SPC);                      // RE -> TRL, SP++
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);                         // RE = x00
  write_microcode_byte(curr_mc_address++, _TRU | _ALU_ADD | _REW);                  // RE += TRU               (add any necessary carrys to [imm16+1])
  write_microcode_byte(curr_mc_address++, _REE | _TUW);                             // RE -> TLU               (TR now contains basically [imm16+1],[imm16] + B reg.)
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _RAW);                      // [TR] -> RA              (now we index into memory with the indexed address and store the data in A reg.)  
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _FLG_BUS | _RFW | _CIC);    // [SP] -> RF              (restore flags)
  // TODO: finish

  // MOV address,RB,RA : RA -> [imm16 + B]
  print_curr_instruction("MOV address,RB,RA", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  write_microcode_byte(curr_mc_address++, _RFE     | _SPE | _MMW);                   // push flags onto stack
  write_microcode_byte(curr_mc_address++, _FLG_CLC | _RFW);                          // clear carry for adding A reg.
  write_microcode_byte(curr_mc_address++, _PCE     | _MME | _ALU_BUS | _REW);        // 
  write_microcode_byte(curr_mc_address++, _ALU_ADD | _REW | _RBE     | _RFW | _PCC); //
  write_microcode_byte(curr_mc_address++, _TLW     | _REE);                          //
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);                          //
  write_microcode_byte(curr_mc_address++, _PCE     | _MME | _ALU_ADD | _REW);        //
  write_microcode_byte(curr_mc_address++, _TUW     | _REE | _PCC);                   //
  write_microcode_byte(curr_mc_address++, _TRE     | _MMW | _RAE);                   // A reg. -> [TR]
  write_microcode_byte(curr_mc_address++, _SPE     | _MME | _FLG_BUS | _RFW | _CIC); // restore flags


  // MOV [address],RB,RA : RA -> [[imm16] + B]
  print_curr_instruction("MOV [address],RB,RA", curr_instruction_opcode);
  write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);

  write_microcode_byte(curr_mc_address++, _RFE | _SPE | _MMW | _SPD);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TLW | _PCC | _SPD | _SPC);
  write_microcode_byte(curr_mc_address++, _PCE | _MME | _TUW | _PCC);
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_BUS | _REW);
  write_microcode_byte(curr_mc_address++, _REE | _SPE | _MMW | _FLG_STC | _RFW);
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);
  write_microcode_byte(curr_mc_address++, _TRL | _ALU_ADD | _REW | _RFW);
  write_microcode_byte(curr_mc_address++, _TLW | _REE);
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);
  write_microcode_byte(curr_mc_address++, _TRU | _ALU_ADD | _REW | _RFW);
  write_microcode_byte(curr_mc_address++, _TUW | _REE);
  write_microcode_byte(curr_mc_address++, _TRE | _MME | _ALU_BUS | _REW);
  write_microcode_byte(curr_mc_address++, _TUW | _REE);
  write_microcode_byte(curr_mc_address++, _RBE | _ALU_BUS | _REW);
  write_microcode_byte(curr_mc_address++, _SPE | _MME | _ALU_ADD | _REW | _RFW);
  write_microcode_byte(curr_mc_address++, _REE | _TLW | _SPC);
  write_microcode_byte(curr_mc_address++, _ALU_x00 | _REW);
  write_microcode_byte(curr_mc_address++, _TRU | _ALU_ADD | _REW);
  write_microcode_byte(curr_mc_address++, _REE | _TUW);
  write_microcode_byte(curr_mc_address++, _TRE | _MMW | _RAE);
  write_microcode_byte(curr_mc_address++, _FLG_BUS | _RFW | _SPE | _MME | _CIC);



  /*||================||*/
  /*||    SPI SHIT    ||*/
  /*||================||*/

  // INB RA,value

  // INB [RCD],RB,val


  /* set unused op codes to HLT */
  Serial.println(F("Writing unsused codes"));

  write_microcode_byte(curr_mc_address, _HLT);
  while (curr_instruction_opcode < 256) {
    write_label_unconditional(IS_OPCODE, curr_instruction_opcode++, curr_mc_address);
  }
  curr_mc_address++;

  /* extra shit */


  unsigned long stop_time = millis();
  int time_spent = (start_time - stop_time) / 1000;
  sprintf(buffer, "took %ds for ROM#%d", time_spent, CURR_ROM_NUM);
  Serial.println(buffer);

  if (num_write_errors > 0) {
    sprintf(buffer, "\r\nERRORS!!: %d\r\n", num_write_errors);
    Serial.println(buffer);
  }
  Serial.println(F("done! :)"));
}

void loop() {}