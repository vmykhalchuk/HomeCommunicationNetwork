// Slave board

// Direct access to ports: https://www.arduino.cc/en/Reference/PortManipulation

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  while (!Serial) {
  }
  //DDRD = DDRD & B00000011; // set D2-D7 as inputs
  //if (digitalRead(A0)) {
    // Enable pull-ups on D0 registers
    for (int i = 2; i < 8; i++) {
      digitalWrite(i, HIGH);
    }
  //}
}

int count = 0;

int sentData_D = 0xFF;
int changedBits_D = 0x00;
int sentData_B = 0xFF;
int changedBits_B = 0x00;
int sentData_C = 0xFF;
int changedBits_C = 0x00;

void inline updatePortsData() {
  //sentData_D = PIND & sentData_D;
  changedBits_D = changedBits_D | (sentData_D ^ PIND);
  changedBits_B = changedBits_B | (sentData_B ^ PIND);
  changedBits_C = changedBits_C | (sentData_C ^ PIND);
}

void summarizeAndReset() {
  sentData_D = sentData_D ^ changedBits_D;
  changedBits_D = 0x00;
  
  sentData_B = sentData_B ^ changedBits_B;
  changedBits_B = 0x00;

  sentData_C = sentData_C ^ changedBits_C;
  changedBits_C = 0x00;
}

void loop() {
  for (int i = 0; i < 1000; i++) {
    delay(1);
    updatePortsData();
  }
  summarizeAndReset();
  int p_D = sentData_D & B11111100;
  //int p_B = sentData_B & 
  //int p_C = 
  
  Serial.write(B10100101); // start token
  Serial.write(p_D);
  Serial.write(~p_D);

  // wait for write operation to complete
  //Serial.flush(); // option 1
  while (Serial.availableForWrite() > 0) { // option 2
    updatePortsData();
  }
  
  count++;
  if (count == 300) {
    digitalWrite(13, HIGH);
  } else if (count == 600) {
    digitalWrite(13, LOW);
    count = 0;
  }
}

