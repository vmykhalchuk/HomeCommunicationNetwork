// Writing bytes:
#define ALL_PREPARE_TO_LISTEN  0xAA
#define DEVICE_NO_1_ID         0xA1
#define COMMAND_RESET_TRIGGERS 0x11
#define COMMAND_GET_TRIGGERS   0x15

//Reading bytes
#define RESPONSE_TO_MASTER     0xAB

// Interface PINs
#define SSerialTxControl       2 //RS485 Direction control
#define RS485Transmit          HIGH
#define RS485Receive           LOW

#define PinLED                 13
#define PinBuzzer              12

void setup() {
  pinMode(SSerialTxControl, OUTPUT);
  digitalWrite(SSerialTxControl, RS485Receive);

  pinMode(PinLED,  OUTPUT);
  pinMode(PinBuzzer,  OUTPUT);

  Serial.begin(4800);
  delay(3000);
}

int readByte() {
  for (int i = 0; i < 100; i++) {
    if (Serial.available()) {
      return Serial.read();
    }
    delay(2);
  }
  return -1;
}

void loop() {
  digitalWrite(SSerialTxControl, RS485Transmit);
  Serial.write(ALL_PREPARE_TO_LISTEN);
  Serial.write(DEVICE_NO_1_ID);
  Serial.write(COMMAND_GET_TRIGGERS);
  if (readByte() == RESPONSE_TO_MASTER) {
    if (readByte() == DEVICE_NO_1_ID) {
      
    }
  } else {
    
  }

  _flush:
  for (int i = 0; i < 500; i++) {
    if (Serial.available()) {
      Serial.read();
    }
    delay(1);
  }
}
