#include <SoftwareSerial.h>

#include <Arduino.h>

// RX0 - this is pin for Serial input #0
// RX1 - this is pin for Serial input #1
// RX2 - this is pin for Serial input #2
// RX3 - this is pin for Serial input #3
// D52 (FIXME - this one is used for MISO!) - this is pin for Serial input #4

// D7 - this is pin for Serial output

// Not all pins on the Mega and Mega 2560 support change interrupts, so only the following can be used for RX: 10, 11, 12, 13, 14, 15, 50, 51, 52, 53, A8 (62), A9 (63), A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69).
const int SSERIAL4_RX_PIN = 52; // FIXME - this one is used for MISO!
const int SSERIALOUT_TX_PIN = 7;

SoftwareSerial SSerial4(SSERIAL4_RX_PIN, 7);
SoftwareSerial SSerialOut(5, SSERIALOUT_TX_PIN);

char* text;
char* number;
String _buffer;

void setup() {
  //Begin serial comunication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(57600);
  while(!Serial);
  Serial1.begin(57600);
  while(!Serial1);
  Serial2.begin(57600);
  while(!Serial2);
  Serial3.begin(57600);
  while(!Serial3);

  SSerialOut.begin(57600); // this one will be used only for writing
  SSerial4.begin(57600); // this one will be actively listening
   
  delay(1000);
   
  SSerialOut.println("L: Setup Complete!");
}

void loop() {
  if (Serial.available())
  {
    String s = Serial.readString();
    printStringToOutput(&s, '0');
  }
  if (Serial1.available())
  {
    String s = Serial1.readString();
    printStringToOutput(&s, '1');
  }
  if (Serial2.available())
  {
    String s = Serial2.readString();
    printStringToOutput(&s, '2');
  }
  if (Serial3.available())
  {
    String s = Serial3.readString();
    printStringToOutput(&s, '3');
  }
  if (SSerial4.available())
  {
    String s = SSerial4.readString();
    printStringToOutput(&s, '4');
  }
}

void printStringToOutput(String* s, char channelC)
{
  SSerialOut.print(channelC); SSerialOut.print(": ");
  int r = 0;
  if (s->endsWith("\x0D\x0A")) {
    r = 2;
  } else if (s->endsWith("\x0D") || s->endsWith("\x0A")) {
    r = 1;
  }

  unsigned int i = 0, s_length = s->length() - r;
  while (i < s_length)
  {
    char c = s->charAt(i);
    int r = 0;
    
    if (c == 0x0A) {
      r = 1; // 0A
    } else if (c == 0x0D) {
      if ( ((i+1) < s_length) && (s->charAt(i+1) == 0x0A) ) {
        r = 2; // 0D,0A
      } else {
        r = 1; // 0D
      }
    }

    if (r == 0) {
      SSerialOut.print(c);
      i++;
    } else {
      SSerialOut.println();
      SSerialOut.print(channelC); SSerialOut.print(": ");
      i += r;
    }
  }

  SSerialOut.println();
}

