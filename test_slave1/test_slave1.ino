#include <SoftwareSerial.h>

#define SSerialRX        7   //Serial Receive pin
#define SSerialTX        9   //Serial Transmit pin
#define SSerialTxControl 8   //RS485 Direction control

#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define PinLED           13

SoftwareSerial RS485Serial(SSerialRX, SSerialTX);
int byteReceived;
int byteSend;

int pOne = 0;
int pTwo = 0;

void setup() {
  pinMode(SSerialRX, INPUT);
  pinMode(SSerialTX, OUTPUT);
  pinMode(SSerialTxControl, OUTPUT);

  pinMode(PinLED, OUTPUT);

  digitalWrite(SSerialTxControl, RS485Receive);
  RS485Serial.begin(4800);//try 4800 if that fails!
}

boolean receivedIdByte = false;
int cc = 0;
void loop() {
  if (cc++ == 32000) {
    digitalWrite(PinLED, HIGH);
    delay(50);
    digitalWrite(PinLED, LOW);
    cc = 0;
  }
  if (RS485Serial.available()) {
    if (receivedIdByte) {
      receivedIdByte = false;
      int command = RS485Serial.read();
      if (command == 0) {
        digitalWrite(PinLED, LOW);
      } else if (command = 1) {
        digitalWrite(PinLED, HIGH);
      } else {
        markError(3);
      }
    } else {
      if (RS485Serial.read() == 0xF1) {
        receivedIdByte = true;
      } else {
        markError(5);
      }
    }
  }
}

void markError(int c) {
  for (int i = 0; i < c; i++) {
    digitalWrite(PinLED, HIGH);
    delay(50);
    digitalWrite(PinLED, LOW);
    delay(150);
  }
}

int rs485ReadByte() {
  if (RS485Serial.available()) {
    return RS485Serial.read();
  }
  return -1;
}

void rs485WriteByte(int byteToWrite) {
digitalWrite(SSerialTxControl, RS485Transmit);
RS485Serial.write(byteToWrite);
digitalWrite(SSerialTxControl, RS485Receive);
}

