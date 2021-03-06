#include <SoftwareSerial.h>

// This is master for two way communication!

SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {
  Serial.begin(9600);
  while (!Serial) {
  }
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

static int COUNTERS_ON_SINGLE_BYTE = 3000; // Adjust for single byte (depending on Serial speed and clock speed)

boolean availableTimedOut(int timeOut) {
  int i = timeOut;
  while(!Serial.available()) {
    if (--i == 0) {
      return false;
    }
  }
  return true;
}

/**
 * slaveNo - from 0 to F
 * p_D port D state (single byte)
 * return error, 0 if OK
 */
int readPortsSlaveState(int slaveNo, int & port_D) {
  while (Serial.available()) {
    Serial.read();
    return 1;
  }
  
  Serial.write(0xA5);
  Serial.write(0xA0);
  Serial.write(0xAF);

  while (Serial.transmitting()) {
    if (Serial.available()) {
      return 2;
    }
  }

  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != 0xA0) { // wrong reply! should be Slave ID!
    return 3;
  }
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE)) {
    return 4; // error!
  }
  int p_D = Serial.read();
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || (byte)Serial.read() != (byte)~p_D) {
    return 5; // error!
  }
  
  Serial.write(p_D);
  Serial.write(0xA6);
  while (Serial.transmitting()) {
    if (Serial.available()) {
      return 6;
    }
  }
  port_D = p_D;
  return 0;
}

void loop() {
  delay(6000);

  mySerial.println("----");
  int port_D = 0;
  int error = readPortsSlaveState(0, port_D);
  if (error != 0) {
    handleError(error);
  }
  mySerial.println(port_D);
}
