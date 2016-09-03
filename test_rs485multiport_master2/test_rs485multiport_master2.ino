#include <SoftwareSerial.h>

// This is master for two way communication!

int outputBufSize = 0;

SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {
  Serial.begin(9600);
  while (!Serial) {
  }
  outputBufSize = Serial.availableForWrite();
  delay(3000);
  while (Serial.available()) {
    Serial.read();
  }
  mySerial.begin(9600);
  handleError(10);
}

void handleError(byte errNo) {
  pinMode(13, OUTPUT);
  for (int i = 0; i < errNo; i++) {
    digitalWrite(13,HIGH);
    delay(150);
    digitalWrite(13,LOW);
    delay(250);
  }
  delay(6000);
}

static int COUNTERS_ON_SINGLE_BYTE = 3000; // Adjust for single byte (depending on Serial speed)

boolean availableTimedOut(int timeOut) {
  int i = timeOut;
  while(!Serial.available()) {
    if (--i == 0) {
      return false;
    }
  }
  return true;
}

void loop() {
  delay(6000);
  while (Serial.available()) {
    Serial.read();
    handleError(1);
  }
  
  Serial.write(0xA5);
  Serial.write(0xA0);
  Serial.write(0xAF);

  int o = 0;
  while(Serial.availableForWrite() != outputBufSize) {
    o++;
  }
  // problem is here!!! it doesn't wait for data to be sent!!!
  // try sending to mySerial to see whats happening here!
  mySerial.write(o);

  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != 0xA0) { // wrong reply! should be Slave ID!
    handleError(2);
    return; // error!
  }
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE)) {
    handleError(3);
    return; // error!
  }
  int p_D = Serial.read();
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || (byte)Serial.read() != (byte)~p_D) {
    handleError(4);
    return; // error!
  }
  
  Serial.write(p_D);
  Serial.write(0xA6);
  while(Serial.availableForWrite()  != outputBufSize) {
  }

  mySerial.write(p_D);
}
