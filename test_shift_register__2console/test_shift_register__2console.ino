
// Interface PINs
#define INPUT_LATCH_PIN        4
#define INPUT_CLOCK_PIN        5
#define INPUT_REGISTER_1_PIN   6
#define INPUT_REGISTER_2_PIN   7
#define INPUT_REGISTER_3_PIN   8

#define PinLED           13
#define PinBuzzer        12

void setup() {
  pinMode(INPUT_LATCH_PIN,  OUTPUT);
  pinMode(INPUT_CLOCK_PIN,  OUTPUT);
  digitalWrite(INPUT_LATCH_PIN, LOW);
  digitalWrite(INPUT_CLOCK_PIN, LOW);

  pinMode(PinLED,  OUTPUT);
  pinMode(PinBuzzer,  OUTPUT);
  
  Serial.begin(9600);
}

int receivedByteReg1 = 0;
int receivedByteReg2 = 0;
int receivedByteReg3 = 0;

void resetStatuses() {
  receivedByteReg1 = 0;
  receivedByteReg2 = 0;
  receivedByteReg3 = 0;
}

void readStatuses() { // every next call will keep already triggered port, unless resetStatuses is called
  digitalWrite(INPUT_LATCH_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(INPUT_LATCH_PIN, LOW);
  for (int i = 0; i < 8; i++) {
    readStatusesBit(i);
  }
}

void readStatusesBit(int i) {
  digitalWrite(INPUT_CLOCK_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(INPUT_CLOCK_PIN, LOW);
  int z = (digitalRead(INPUT_REGISTER_1_PIN) ? 1 : 0) << i;
  receivedByteReg1 |= z;
  z = (digitalRead(INPUT_REGISTER_2_PIN) ? 1 : 0) << i;
  receivedByteReg2 |= z;
  z = (digitalRead(INPUT_REGISTER_3_PIN) ? 1 : 0) << i;
  receivedByteReg3 |= z;
}

void readStatuses_1() { // same as readStatuses, however split on two functions to make shorter ticks
  digitalWrite(INPUT_LATCH_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(INPUT_LATCH_PIN, LOW);
  for (int i = 0; i < 3; i++) {
    readStatusesBit(i);
  }
}

int buzzerStatus = LOW;
void readStatuses_2() {
  for (int i = 3; i < 8; i++) {
    readStatusesBit(i);
  }
  if (receivedByteReg2 != 0) {
    digitalWrite(PinLED, HIGH);
  }
  digitalWrite(PinBuzzer, buzzerStatus); buzzerStatus = (buzzerStatus == LOW) ? HIGH : LOW;
}

boolean readStatusesStep1 = true;
#define tick() { if (readStatusesStep1) readStatuses_1(); else readStatuses_2(); readStatusesStep1 = !readStatusesStep1; }

void writeByte(int byteToWrite) {
  Serial.write(byteToWrite);
}

void loop() {
  resetStatuses();
  //wait - no hurry to reply
  for (int i = 0; i < 200; i++) {
    delayMicroseconds(500);
    tick();
  }
  writeByte(0xFF);
  writeByte(0x00);
  writeByte(0xFF);
  writeByte(receivedByteReg1);
  writeByte(receivedByteReg2);
  writeByte(receivedByteReg3);
}
