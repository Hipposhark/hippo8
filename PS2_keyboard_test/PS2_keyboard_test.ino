// #include <PS2Keyboard.h>

// const int DataPin = 4;
// const int IRQpin  = 8;

// PS2Keyboard keyboard;

// void setup() {
//   delay(1000);
//   keyboard.begin(DataPin, IRQpin);
//   Serial.begin(9600);
//   Serial.println("Keyboard Test:");
// }

// void loop() {
//   if (keyboard.available()) {
    
//     // read the next key
//     char c = keyboard.read();
    
//     // check for some of the special keys
//     if (c == PS2_ENTER) {
//       Serial.println();
//     } else if (c == PS2_TAB) {
//       Serial.print("[Tab]");
//     } else if (c == PS2_ESC) {
//       Serial.print("[ESC]");
//     } else if (c == PS2_PAGEDOWN) {
//       Serial.print("[PgDn]");
//     } else if (c == PS2_PAGEUP) {
//       Serial.print("[PgUp]");
//     } else if (c == PS2_LEFTARROW) {
//       Serial.print("[Left]");
//     } else if (c == PS2_RIGHTARROW) {
//       Serial.print("[Right]");
//     } else if (c == PS2_UPARROW) {
//       Serial.print("[Up]");
//     } else if (c == PS2_DOWNARROW) {
//       Serial.print("[Down]");
//     } else if (c == PS2_DELETE) {
//       Serial.print("[Del]");
//     } else {
      
//       // otherwise, just print all normal characters
//       Serial.print(c);
//     }
//   } else {
//     Serial.print("NOT AVAILABLE");
//   }
// }


#define CLOCK 3
#define DATA  8

void setup() {
  Serial.begin(115200);
  pinMode(CLOCK, INPUT);
  pinMode(DATA,  INPUT);
}

void loop() {
  uint16_t scanval;
  for (int i=0; i<11; i++) {
    while (digitalRead(CLOCK));
    scanval |= digitalRead(DATA) << i;
    while (!digitalRead(DATA));
  }
  scanval >>= 1;    // remove start bit
  // scanval &=  0xFF; // mask out stop bit and parity
  Serial.println(scanval, HEX);
}