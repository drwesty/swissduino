/*
  Arduino Meets Linux:
  The User's Guide to Arduino Yún Development
  Copyright (c) 2015, Bob Hammell.

  Disclaimer:
  This code is provided by ArduinoMeetsLinux.com on an "AS IS"
  basis. ArduinoMeetsLinux.com makes no warranties, express or
  implied, regarding use of the code alone or in combination with
  your projects.

  In no event shall ArduinoMeetsLinux.com be liable for any special,
  indirect, incidental, or consequential damages (including, but not
  limited to, loss of data and damage to systems) arising in any way
  out of the use, reproduction, modification and/or distribution of
  this code.
*/

// http://forum.arduino.cc/index.php?topic=361409.0 Faster serial?

#include <Console.h>
#include <HID-Project.h>
#include <HID-APIs/ImprovedKeylayouts.h>

#define USB_VID 0x046d
#define USB_PID 0xc313
#define USB_MANUFACTURER "Logitech, Inc."
#define USB_PRODUCT "Internet 350 Keyboard"

#define WINDOWS

#define EV_KEY         0x0100
#define EV_RAW         0x0200
#define EV_HID         0x0300
#define EV_RELEASED    0x00000000
#define EV_PRESSED     0x01000000
#define EV_REPEAT      0x02000000

#define LED            13
#define RAW_EXIT       0xFF
#define RAW_FILENAME   0x10
#define RAW_DATA       0x11
#define RAW_SIZE       0x12
#define RAW_MET        0x04
#define RAW_METCTL     0x05

struct raw_data {
  uint8_t type;
  char data[63];
};

struct raw_data msfdata,hiddata;

struct input_event {
  uint16_t type;
  uint16_t code;
  int32_t value;
};
struct input_event event;

Process p;
const KeyboardKeycode rq[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEYPAD_5, KEYPAD_6};
const KeyboardKeycode ra[] = {KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, HID_KEYBOARD_QUOTE_AND_DOUBLEQUOTE, KEY_BACKSLASH};
const KeyboardKeycode rz[] = {KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_PERIOD, KEY_SLASH};
const KeyboardKeycode rf[] = {KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10};
const KeyboardKeycode rn[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0};
char buffer[512];
int buffer_len = 0;
int ctrl_held = 0;



KeyboardKeycode convertKey(uint16_t k) {
  uint8_t key = (k & 0xFF00) >> 8;
  switch (key) {
    case 1:
      return KEY_ESC;
    case 12:
      return KEY_MINUS;
    case 13:
      return KEY_EQUAL;
    case 14:
      return KEY_BACKSPACE;
    case 15:
      return KEY_TAB;
    case 28:
      return KEY_RETURN;
    case 29:
      return KEY_LEFT_CTRL;
    case 42:
      return KEY_LEFT_SHIFT;
    case 43:
      return KEY_BACKSLASH;
    case 54:
      return KEY_RIGHT_SHIFT;
    case 56:
      return KEY_LEFT_ALT;
    case 57:
      return KEY_SPACE;
    case 58:
      return KEY_CAPS_LOCK;
    case 86:
      // American guff
      //return KEY_BACKSLASH;
      return HID_KEYBOARD_NON_US_BACKSLASH_AND_PIPE;
    case 87:
      return KEY_F11;
    case 88:
      return KEY_F12;
    case 96:
      return KEY_RETURN;
    case 97:
      return KEY_RIGHT_CTRL;
    case 100:
      return KEY_RIGHT_ALT;
    case 102:
      return KEY_HOME;
    case 103:
      return KEY_UP_ARROW;
    case 104:
      return KEY_PAGE_UP;
    case 105:
      return KEY_LEFT_ARROW;
    case 106:
      return KEY_RIGHT_ARROW;
    case 107:
      return KEY_END;
    case 108:
      return KEY_DOWN_ARROW;
    case 109:
      return KEY_PAGE_DOWN;
    case 110:
      return KEY_INSERT;
    case 111:
      return KEY_DELETE;
    case 125:
      return KEY_LEFT_GUI;
    case 127:
      return KEY_RIGHT_GUI;
    default:
      if ( (key >= 59) && (key <= 68) ) {
        return rf[key - 59];
      }
      else if ( (key >= 2) && (key <= 11) ) {
        return rn[key - 2];
      }
      else if (key == 11) {
        return KEY_0;
      }
      else if ( (key >= 16) && (key <= 27) ) {
        return rq[key - 16];
      }
      else if ( (key >= 30) && (key <= 41) ) {
        return ra[key - 30];
      }
      else if ( (key >= 44) && (key <= 53) ) {
        return rz[key - 44];
      }
      else {
        return KEY_RESERVED;
      }
  }
}

void setup() {
  delay(1000);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  //Bridge.begin();

  //p.runShellCommand(F("killall kbd.py"));
  //p.runShellCommandAsynchronously(F("/mnt/sda1/swissduino/kbd.py"));
  //Console.begin();
  Serial1.begin(9600);

  //while (!Console) {
  while (!Serial1.available()) {
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
  }
}

void loop() {
  int i, j;
  char s[5];
  delay(5);
  //while (!Console) {
  //Serial.print("loop..");
  //Serial.print(Serial1.available());
  //  if (Console.available() >= sizeof(event)) {
  //    Console.readBytes((char *)&event, sizeof(event));
  if (Serial1.available() >= sizeof(event)) {
    Serial1.readBytes((char *)&event, sizeof(event));


    if (event.type == EV_KEY) {
      KeyboardKeycode k = convertKey(event.code);
      if ( (event.value == EV_PRESSED) && (k != 0) ) {
        Keyboard.press(k);
      }
      else if ( (event.value == EV_RELEASED) && (k != 0) ) {
        Keyboard.release(k);
      }
    }
    else if (event.type == EV_RAW) {

      // Fuck around with keyboard conversion - take ASCII code and map to keycode
      // WHY DO I HAVE TO DO THIS

      // double Quotes need to be done for some have to provide raw code (cos its shite)
      if (event.code == 35) {
        Keyboard.press(convertKey(11008));
        Keyboard.release(convertKey(11008));
      }
      else if (event.code == 92) {
        Keyboard.press(convertKey(22016));
        Keyboard.release(convertKey(22016));
      }
      else if (event.code == 34) {
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(convertKey(768));
        Keyboard.release(convertKey(768));
        Keyboard.release(KEY_LEFT_SHIFT);
      }
      else if (event.code == 124) { // Bar - ok
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(HID_KEYBOARD_NON_US_BACKSLASH_AND_PIPE);
        Keyboard.release(HID_KEYBOARD_NON_US_BACKSLASH_AND_PIPE);
        Keyboard.release(KEY_LEFT_SHIFT);
      }
      else if (event.code == 126) { // ~
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_BACKSLASH);
        Keyboard.release(KEY_BACKSLASH);
        Keyboard.release(KEY_LEFT_SHIFT);
      }
      else if (event.code == 64) { // @
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_QUOTE);
        Keyboard.release(KEY_QUOTE);
        Keyboard.release(KEY_LEFT_SHIFT);
      }
      else {
        Keyboard.press(event.code);
        Keyboard.release(event.code);
      }


    }
    else if (event.type == EV_HID) {
      // TODO Set blocksize based on mode/OS
      Serial.print("Entering EV_HID mode\n");
      // Enable HID mode
      Keyboard.releaseAll();
      RawHID.begin(buffer, sizeof(buffer));
      // Enter main HID transfer loop
      digitalWrite(LED, HIGH);
      hid_loop();
      // Disable HID mode
      RawHID.end();
      digitalWrite(LED, LOW);
    }

  }
}



void hid_loop() {
  uint16_t iSend;
  int retry = 3;
  int backoff = 100;
  int packets = 0;
  char textString[16];

  while (1) {

    // Have short delay here?
    // delay(5);

    if (RawHID.available()) {
      RawHID.readBytes((uint8_t *)&hiddata, sizeof(hiddata));
      //DumpHex("HID",(unsigned char *)(&hiddata));
      if (hiddata.type == RAW_EXIT) {
        Serial.print("HID Exiting!\n");
        return;
      }
      // Send onwards - just block...
      iSend = -1;
      while (iSend == -1) {
        iSend = Serial1.write((uint8_t *)&hiddata, sizeof(hiddata));
        if (iSend == -1) {
          Serial.print("Blocked to Yun\n");
          delay(backoff);
        }
      }
    }

    // Receive from Yun first 
    // 04-08-18 switched 
    if (Serial1.available()) {

      Serial1.readBytes((uint8_t *)&msfdata, sizeof(msfdata));
      //DumpHex("MSF",(unsigned char *)(&msfdata));
   
      if (msfdata.type == RAW_EXIT) {
        Serial.print("Exiting!\n");
        return;
      }
      // Send onwards - just block...
      iSend = -1;
      while (iSend == -1) {
        iSend = RawHID.write((uint8_t *)&msfdata, sizeof(msfdata));
        if (iSend == -1) {
          Serial.print("Blocked to HID\n");
          delay(backoff);
        }
      }

    }


  }
}

static void  DumpHex(char *title,unsigned char *data)
{
  int       ii;
  unsigned char  theValue;
  char      textString[16];
  char      asciiDump[64];
  unsigned  char *myAddressPointer;

  myAddressPointer  = data;

  sprintf(textString, "%s:%04X - ", title, myAddressPointer);
  Serial.print(textString);

  asciiDump[0]    = 0;
  for (ii = 0; ii < 64; ii++)
  {
    // theValue  = pgm_read_byte_near(myAddressPointer);
    theValue  = *myAddressPointer;

    sprintf(textString, "%02X ", theValue);
    Serial.print(textString);
    if ((theValue >= 0x20) && (theValue < 0x7f))
    {
      asciiDump[ii % 64]  = theValue;
    }
    else
    {
      asciiDump[ii % 64]  = '.';
    }

    myAddressPointer++;
  }
  asciiDump[64] = 0;
  Serial.print(asciiDump);
  Serial.print("\n");

}
