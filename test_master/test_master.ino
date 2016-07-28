#include <SoftwareSerial.h>

#define SSerialRX        7   //Serial Receive pin
#define SSerialTX        9   //Serial Transmit pin
#define SSerialTxControl 8   //RS485 Direction control

#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define PinLED           13
#define PinBuzzer        12

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
  pinMode(PinBuzzer, OUTPUT);

  digitalWrite(SSerialTxControl, RS485Receive);
  RS485Serial.begin(4800);//try 4800 if that fails!
}

void loop() {
  rs485writeByte(0xF1);
  rs485writeByte(0x01);
  buzz();
  delay(1000);
  rs485writeByte(0xF1);
  rs485writeByte(0x00);
  buzz();
  delay(1000);
}

void rs485writeByte(int byteToSend) {
  digitalWrite(SSerialTxControl, RS485Transmit);
  RS485Serial.write(byteToSend);
  digitalWrite(SSerialTxControl, RS485Receive);
}

void buzz() {
  for (int i = 0; i <1024; i++) {
    digitalWrite(PinBuzzer, HIGH);
    delayMicroseconds(100);
    digitalWrite(PinBuzzer, LOW);
    delayMicroseconds(100);
  }
}

void _loop() {
  pOne = procedureOne(pOne);
  if (pOne == -1) {
    pOne = procedureOne(0);
  }
  pTwo = procedureTwo(pTwo);
  if (pTwo == -1) {
    pTwo = procedureTwo(0);
  }
}

int procedureOne(int step) {
  switch(step) {
    case 0:
      //
    break; case 1:
      //
    break; case 2:
      //
    break; default: return -1;
  }
  return step++;
}

int procedureTwo(int step) {
  switch(step) {
    case 0:
      //
    break; case 1:
      //
    break; case 2:
      //
    break; default: return -1;
  }
  return step++;
}

