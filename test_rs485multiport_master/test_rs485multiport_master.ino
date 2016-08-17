// Master board

// Direct access to ports: https://www.arduino.cc/en/Reference/PortManipulation

//#include <SoftwareSerial.h>

//SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);// change to HIGH
  Serial.begin(9600);
  while (!Serial) {
  }
  DDRD = DDRD | B11111100; // set D2-D7 as outputs
  //Serial.setTimeout(5000);
  //mySerial.begin(9600);
}

byte buff[100];
int errors = 0;
void handleError(byte errNo) {
  //mySerial.println("Err:" + errNo);
  //mySerial.flush();
  for (int i = 0; i < errNo; i++) {
    digitalWrite(13,HIGH);
    delay(150);
    digitalWrite(13,LOW);
    delay(250);
  }
  delay(6000);
  
  /*if (errors == 5) {
    errors++;
    digitalWrite(13, HIGH);
  } else {
    errors++;
  }*/
}

boolean availableTimedOut() {
  int timeOut = 5000;
  while(!Serial.available()) {
    timeOut--;
    if (timeOut == 0) {
      return true;
    }
    delay(1);
  }
  return false;
}

void loop() {
  if (availableTimedOut()) {
    handleError(1); // timedout on reception
    return;
  }
  int b0 = Serial.read();
  
  if (b0 != B10100101) {
    handleError(2); // wrong start byte!
    return;
  }

  if (availableTimedOut()) {
    handleError(2); // no second byte
  }
  int b1 = Serial.read();
  if (availableTimedOut()) {
    handleError(3); // no third byte
  }
  int b2 = Serial.read();
  
  if ((b1 & B00000011) != 0) {
    handleError(4); // error in reception of second byte
    return;
  }

  if ((byte)b1 != (byte)~b2) {
    handleError(5); // error check failed!
    return;
  }

  // success!!!
  errors = 0;
  digitalWrite(13, LOW);
  PORTD = (PORTD & B11) | b1;
}

