const int pin_SER = 10;
const int pin_RCLK = 4;
const int pin_SRCLK = 5;

void setup() {
  pinMode(pin_SER, OUTPUT);
  pinMode(pin_RCLK, OUTPUT);
  pinMode(pin_SRCLK, OUTPUT);

  digitalWrite(pin_SER, LOW);
  digitalWrite(pin_RCLK, LOW);
  digitalWrite(pin_SRCLK, LOW);

  // reset chip
  for (int i = 0; i < 10; i++) {
    clockSerial();
  }
  clockOutput();
}

void clockSerial() {
  digitalWrite(pin_SRCLK, HIGH);
  delay(1);
  digitalWrite(pin_SRCLK, LOW);
  delay(1);
}

void clockOutput() {
  digitalWrite(pin_RCLK, HIGH);
  delay(1);
  digitalWrite(pin_RCLK, LOW);
  delay(1);
}

void loop() {
  loop_running();
}

int digNo = 0;
void loop_running() {
  if (digNo == 0) {
    digitalWrite(pin_SER, HIGH);
    clockSerial();
    digitalWrite(pin_SER, LOW);
  //} else {
  //  digitalWrite(pin_SER, LOW);
  }
  digNo++;
  
  clockSerial();
  clockOutput();

  if (digNo == 6) {
    digNo = 0;
  }

  delay(100);
}

void loop_all_blink() {
  // reset chip
  digitalWrite(pin_SER, LOW);
  for (int i = 0; i < 10; i++) {
    clockSerial();
  }
  clockOutput();

  delay(1000);
  
  // set all output to HIGH
  digitalWrite(pin_SER, HIGH);
  for (int i = 0; i < 10; i++) {
    clockSerial();
  }
  clockOutput();

  delay(1000);
}
