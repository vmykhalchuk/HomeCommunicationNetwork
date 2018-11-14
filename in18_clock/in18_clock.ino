#include <DS3231.h>

// output to shift registers (to output proper digit)
const int pin_SER = 10;
const int pin_RCLK = 4;
const int pin_SRCLK = 5;

// Dots between 
const int pin_DOT1 = 6;
const int pin_DOT2 = 7;
const int pin_DOT3 = 8;
const int pin_DOT4 = 9;

// connect DS3231 to ARDUINO
//          SDA       A4
//          SCL       A5
// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);

// Init a Time-data structure
Time  t;

// interrupt pin for clock square output (SQW pin of DS3231)
const int pin_SQW = 11;

void setup() {
  pinMode(pin_SER, OUTPUT);
  pinMode(pin_RCLK, OUTPUT);
  pinMode(pin_SRCLK, OUTPUT);

  pinMode(pin_DOT1, OUTPUT);
  pinMode(pin_DOT2, OUTPUT);
  pinMode(pin_DOT3, OUTPUT);
  pinMode(pin_DOT4, OUTPUT);

  digitalWrite(pin_SER, LOW);
  digitalWrite(pin_RCLK, LOW);
  digitalWrite(pin_SRCLK, LOW);

  digitalWrite(pin_DOT1, LOW);
  digitalWrite(pin_DOT2, LOW);
  digitalWrite(pin_DOT3, LOW);
  digitalWrite(pin_DOT4, LOW);

  // reset shift out chip
  for (int i = 0; i < 10; i++) {
    clockSerial();
  }
  clockOutput();

  // Initialize the rtc object
  rtc.begin();

  // enable SQW output and set it to 1Hz
  rtc.setOutput(true);
  rtc.setSQWRate(0);

  Serial.begin(57600);
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
  //loop_running();
  //loop_ReadClock();
  loop_test_readClockSpeed();
}

void loop_test_readClockSpeed() {
  unsigned long start = millis();
  int c = 0;
  unsigned long end = 0;
  while ((end = millis()) - start < 1000) {
    rtc.getTime();
    c++;
  }
  Serial.print("Times measured: ");
  Serial.println(c, DEC);
  Serial.println(start, DEC);
  Serial.println(end, DEC);
  Serial.flush();
  if (c == 0) {
    delay(2000);
  }
}

int loop_ReadClock_c = 0;
void loop_ReadClock() {
  if (loop_ReadClock_c++ < 50) {
    return;
  }
  loop_ReadClock_c = 0;
  Serial.print("Temperature: ");
  Serial.print(rtc.getTemp());
  Serial.println(" C");
  t = rtc.getTime();
  // Send date over serial connection
  Serial.print("Today is the ");
  Serial.print(t.date, DEC);
  Serial.print(". day of ");
  Serial.print(rtc.getMonthStr());
  Serial.print(" in the year ");
  Serial.print(t.year, DEC);
  Serial.println(".");
  
  // Send Day-of-Week and time
  Serial.print("It is the ");
  Serial.print(t.dow, DEC);
  Serial.print(". day of the week (counting monday as the 1th), and it has passed ");
  Serial.print(t.hour, DEC);
  Serial.print(" hour(s), ");
  Serial.print(t.min, DEC);
  Serial.print(" minute(s) and ");
  Serial.print(t.sec, DEC);
  Serial.println(" second(s) since midnight.");

  // Send a divider for readability
  Serial.println("  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
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

