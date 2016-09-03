#include <SoftwareSerial.h>

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

int c = 0;
int s = 9600;

void loop() {
  mySerial.println("start");
  int bytesToWrite = (++c);
  c = c % 12;
  if (c == 0) {
    s = s == 9600 ? 1200 : 9600;
    Serial.end();
    Serial.begin(s);
    while (!Serial) {}
    mySerial.print("new speed: ");
    mySerial.println(s, DEC);
  }
  mySerial.print("b2w=");
  mySerial.println(bytesToWrite, DEC);
  mySerial.flush();

  for (int i = 0; i < bytesToWrite; i++) {
    Serial.write(0xFF);
  }

  /*int z = 0;
  while(Serial.availableForWrite() != outputBufSize) {
    z++;
  }*/
  /*int z = millis();
  Serial.flush();
  z = millis() - z;*/
  int z = 0;
  while (Serial.flush2()) {
    z++;
  }

  mySerial.print("Z="); // make sure that this counter has some value for single byte and twice as much for two bytes and so on...
  mySerial.println(z, DEC);
  mySerial.println("end");
  mySerial.println();
  //mySerial.flush();
  delay(6000);
}
