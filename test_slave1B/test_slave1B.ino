#include <SoftwareSerial.h>

#define SSerialRX        7   //Serial Receive pin
#define SSerialTX        9   //Serial Transmit pin
#define SSerialTxControl 8   //RS485 Direction control

#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define PinLED           13

//SoftwareSerial RS485Serial(SSerialRX, SSerialTX);
int byteReceived;
int byteSend;

int pOne = 0;
int pTwo = 0;

void setup() {
  //pinMode(SSerialRX, INPUT);
  //pinMode(SSerialTX, OUTPUT);
  pinMode(SSerialTxControl, OUTPUT);

  pinMode(PinLED, OUTPUT);

  digitalWrite(SSerialTxControl, RS485Receive);
  //RS485Serial.begin(4800);//try 4800 if that fails!
  Serial.begin(4800);
}

boolean receivedIdByte = false;
int cc = 0;
int cc2 = 0;
void loop() {
  if (cc++ == 32000) {
    if (cc2++ == 10) {
      digitalWrite(PinLED, HIGH);
      delay(50);
      digitalWrite(PinLED, LOW);
      cc2 = 0;
    }
    cc = 0;
  }
  if (Serial.available()) {
    if (receivedIdByte) {
      receivedIdByte = false;
      int command = Serial.read();
      if (command == 0) {
        digitalWrite(PinLED, LOW);
      } else if (command = 1) {
        digitalWrite(PinLED, HIGH);
      } else {
        markError(4);
      }
    } else {
      if (Serial.read() == 0xF1) {
        receivedIdByte = true;
      } else {
        markError(2);
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
  delay(1000);
}
