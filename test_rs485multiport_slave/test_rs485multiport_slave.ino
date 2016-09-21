// Slave board

// Direct access to ports: https://www.arduino.cc/en/Reference/PortManipulation
// Serial.transmitting() - return false if transmission is over or true if still transmitting

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  while (!Serial) {
  }
  delay(3000);
  while (Serial.available()) {
    Serial.read();
  }
  
  // set D2-D7 as inputs
  DDRD &= B00000011;
  // Enable pull-ups on D0 registers
  for (int i = 2; i < 8; i++) { digitalWrite(i, HIGH); }

  handleError(7);
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

int count = 0;
void loop__() {
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
  while (Serial.transmitting()) { // option 2 (requires modified Arduino)
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


static int COUNTERS_ON_SINGLE_BYTE = 2000; // Adjust for single byte (depending on Serial speed)
boolean lastSentFailed = false;// send previous data due to transmission error!
int p_D = 0;

boolean availableTimedOut(int timeOut) {
  int i = timeOut;
  while(!Serial.available()) {
    updatePortsData();
    if (--i == 0) {
      return false;
    }
  }
  return true;
}

/**
 * This loop is waiting for a request from Master in order to send its status.
 * Communication protocol is following:
 * 
 * M: 0xA5 - initiate transmission, all slaves - listen!
 * M: 0xAn, n - ID of slave to listen
 * M: ~0xAn
 * 
 * S: 0xAn
 * S: {p_D}
 * S: ~{p_D}
 * 
 * M: {p_D}
 * M: 0xA6 - receipt acknowledge
 */

void loop() {
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != 0xA5) { // start communication token (all slaves - listen!)
    //handleError(1);
    return;
  }
  
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != 0xA0) { // My ID is 0, 0xAn - where n is slave ID, up to 16 Slaves support
    return;
    // improved mechanism can be where we actively read communication with other slave, so we do not accidentaly engage into it. For example we can read that ID is not us, but we read and monitor all transmission, and see if it stops or corrupts, so we can only then start monitoring for beginning of new transmission which can be directed to us
  }
  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != 0xAF) { // repeat ID inverted, for transmission check!
    handleError(2);
    return;
  }

  // now wait a little and send back status
  if (availableTimedOut(100)) {
    // we do not expect anything on Serial!
    handleError(3);
    return;
  }

  if (!lastSentFailed) {
    summarizeAndReset();
    p_D = sentData_D & B11111100;
    //int p_B = sentData_B & 
    //int p_C = 
  }
  Serial.write(0xA0); // send my ID
  Serial.write(p_D);  // send data
  Serial.write(~p_D); // send data inverted

  while (Serial.transmitting()) {
    updatePortsData();
  }

  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != p_D) { // Receipt acknowledged (with received data)
    handleError(4);
    lastSentFailed = true;
    return;
  }

  if (!availableTimedOut(COUNTERS_ON_SINGLE_BYTE) || Serial.read() != 0xA6) { // Receipt acknowledged
    handleError(5);
    lastSentFailed = true;
    return;
  }

  lastSentFailed = false;
}

