#include <SoftwareSerial.h>

#include <Arduino.h>

// RX0 - this is pin for Serial input #M
// D3 - this is pin for Serial input #1
// D6 - this is pin for Serial output

const int SSERIAL1_RX_PIN = 3;
const int SSERIALOUT_RX_PIN = 5;

SoftwareSerial SSerial1(SSERIAL1_RX_PIN, 4);
SoftwareSerial SSerialOut(SSERIALOUT_RX_PIN, 6);

char* text;
char* number;
String _buffer;

void setup() {
  //Begin serial comunication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(57600);
  while(!Serial);

  SSerialOut.begin(57600); // this one will be used only for writing
  SSerial1.begin(57600); // this one will be actively listening
   
  delay(1000);
   
  SSerialOut.println("D: Setup Complete!");
}

void loop() {
  if (Serial.available())
  {
    String s = Serial.readString();
    SSerialOut.print("M: "); SSerialOut.println(s);
  }
  if (SSerial1.available())
  {
    String s = SSerial1.readString();
    SSerialOut.print("1: "); SSerialOut.println(s);
  }
}
