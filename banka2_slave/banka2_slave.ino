#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include "RF24.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

const byte zoomerPin = 5; // cannot be 13 since radio is using it!

const byte interruptPinA = 2;
const byte interruptPinB = 3;
volatile byte interruptsOnPinA = 0;
volatile byte interruptsOnPinB = 0;

/* Assign a unique ID to this sensor at the same time */
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

void pinAInterruptRoutine(void)
{
  if (interruptsOnPinA <= 0xD) {
    interruptsOnPinA++;
  } else {
    interruptsOnPinA = 0xF;
    detachInterrupt(digitalPinToInterrupt(interruptPinA));
  }
}

void pinBInterruptRoutine(void)
{
  if (interruptsOnPinB <= 0xD) {
    interruptsOnPinB++;
  } else {
    interruptsOnPinB = 0xF;
    detachInterrupt(digitalPinToInterrupt(interruptPinB));
  }
}

volatile byte f_wdt=1;
volatile byte wdt_overruns = 0;
ISR(WDT_vect)
{
  if (f_wdt == 0)
  {
    f_wdt=1;
  }
  else if (f_wdt == 1)
  {
    // we run into wdt overrun without processor not responding
    f_wdt=2;
  }
  else
  {
    // another overrun, now lets reset mcu
    // Here we should reset CPU since this is a cause of unexpected behaviour!
    wdt_overruns++;
    Serial.println("WDT Overrun!!!");
    Serial.flush();
  }
}

inline void enterSleepAndAttachInterrupt(void)
{
  cli();
  /* Setup pin2 as an interrupt and attach handler. */
            // digitalPinToInterrupt(2) => 0
            // digitalPinToInterrupt(3) => 1
            // digitalPinToInterrupt(1 | 4) => -1
  attachInterrupt(digitalPinToInterrupt(interruptPinA), pinAInterruptRoutine, LOW); // can be LOW / CHANGE / HIGH / ... FALLING
  attachInterrupt(digitalPinToInterrupt(interruptPinB), pinBInterruptRoutine, LOW);

  enterSleep();
}

inline void enterSleep(void)
{
  cli();
  // allowed modes:
  // SLEEP_MODE_IDLE         (0)
  // SLEEP_MODE_ADC          _BV(SM0)
  // SLEEP_MODE_PWR_DOWN     _BV(SM1)
  // SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
  // SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))
  // SLEEP_MODE_EXT_STANDBY  (_BV(SM0) | _BV(SM1) | _BV(SM2))
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  sleep_enable();
  sei(); //after sei we have one clock cycle to execute any command before interrupt will be activated
  sleep_cpu(); /* The program will continue from here. */ // it was : sleep_mode() , seems to be same functionality
  sleep_disable();
}

typedef enum { WDT_PRSCL_16ms = 0, WDT_PRSCL_32ms, WDT_PRSCL_64ms, WDT_PRSCL_125ms, 
    WDT_PRSCL_250ms, WDT_PRSCL_500ms, WDT_PRSCL_1s, WDT_PRSCL_2s, WDT_PRSCL_4s, WDT_PRSCL_8s } edt_prescaler ;

void setupWdt(boolean enableInterrupt, uint8_t prescaler)
{
  byte wdtcsrValue = enableInterrupt ? 1<<WDIE : 0;
  wdtcsrValue |= (prescaler & 1) ? 1<<WDP0 : 0;
  wdtcsrValue |= (prescaler & 2) ? 1<<WDP1 : 0;
  wdtcsrValue |= (prescaler & 4) ? 1<<WDP2 : 0;
  wdtcsrValue |= (prescaler & 8) ? 1<<WDP3 : 0;
  
  cli();
  /*** Setup the WDT ***/
  /* Clear the reset flag. */
  MCUSR &= ~(1<<WDRF); // chapter 11.9
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE); // chapter 11.9 and scroll up
  /* set new watchdog timeout prescaler value */
  // WDP0 & WDP3 = 8sec
  // WDIE = enable interrupt (note no reset)
  //WDTCSR = 1<<WDIE | 1<<WDP0 | 1<<WDP3; /* 8.0 seconds */
  WDTCSR = wdtcsrValue;
  sei();
}

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);
byte addressMaster[6] = "MBank";
byte addressSlave[6] = "1Bank";

boolean setupRadio(void)
{
  if (!radio.begin())
  {
    return false;
  }
  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(118); // 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  radio.openWritingPipe(addressMaster);
  radio.openReadingPipe(1, addressSlave);

  return true;
}

volatile boolean isRadioUp = true;
void putRadioDown()
{
  if (isRadioUp) {
    radio.powerDown();
  }
  isRadioUp = false;
}
void putRadioUp()
{
  if (!isRadioUp) {
    radio.powerUp();
  }
  isRadioUp = true;
}

void setup()
{
  Serial.begin(9600);
  while(!Serial) {}

  pinMode(zoomerPin, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(zoomerPin, HIGH);
    delay(1000);
    digitalWrite(zoomerPin, LOW);
    delay(1000);
  }
  
  /* Setup the pin direction. */
  pinMode(interruptPinA, INPUT);
  pinMode(interruptPinB, INPUT);

  if (false)
  { // don't know how to do it
    // disable BOD
    /*sbi(MCUCR,BODS);
    sbi(MCUCR,BODSE);
    sbi(MCUCR,BODS);
    cbi(MCUCR,BODSE);*/
    // it must be done before actual sleep! see p.45
    MCUCR |= (1<<BODS) | (1<<BODSE);
    MCUCR |= (1<<BODS);
    MCUCR &= ~(1<<BODSE);
  }

  if (false)
  { // didn't help at all - still no power reduction in sleep mode
    // disable ADC
    ADCSRA = 0;
    // in order to recover ADC after sleep - write ADCSRA to memory and restore it after wakeup
  }

  setupWdt(true, WDT_PRSCL_8s);

  if (!setupRadio()) {
    while(true) {
      Serial.println("Radio Initialization failed!!!");
      for (int i = 0; i < 3; i++) {
        digitalWrite(zoomerPin, HIGH);
        delay(500);
        digitalWrite(zoomerPin, LOW);
        delay(500);
        wdt_reset();
      }
      delay(2000);
      wdt_reset();
    }
  }

  if(!mag.begin())
  {
    while(true) {
      Serial.println("Magnetometer Initialization failed!!!");
      for (int i = 0; i < 2; i++) {
        digitalWrite(zoomerPin, HIGH);
        delay(500);
        digitalWrite(zoomerPin, LOW);
        delay(500);
        wdt_reset();
      }
      delay(2000);
      wdt_reset();
    }
  }

  Serial.println("Initialization complete.");
  for (int i = 0; i < 5; i++) {
    wdt_reset();
    digitalWrite(zoomerPin, HIGH);
    delay(500);
    wdt_reset();
    digitalWrite(zoomerPin, LOW);
    delay(500);
  }
  
}

byte magState = 0;

byte transmitNowCounter = 0;
byte lastTransmitFailed = 0;

byte transmission[7];
byte transmissionNo = 0;
void initializeTransmission()
{
    byte _intsOnPinA, _intsOnPinB;
    { // read values and attach interrupts back again (in case they were detached due to overflow)
      cli();
      _intsOnPinA = interruptsOnPinA;
      interruptsOnPinA = 0;
      _intsOnPinB = interruptsOnPinB;
      interruptsOnPinB = 0;
      /* Setup pin2 as an interrupt and attach handler. */
            // digitalPinToInterrupt(2) => 0
            // digitalPinToInterrupt(3) => 1
            // digitalPinToInterrupt(1 | 4) => -1
      attachInterrupt(digitalPinToInterrupt(interruptPinA), pinAInterruptRoutine, LOW); // can be LOW / CHANGE / HIGH / ...
      attachInterrupt(digitalPinToInterrupt(interruptPinB), pinBInterruptRoutine, LOW);
      sei();
    }
    
    transmission[0] = 0x11; // ID of this Banka(r)
    transmission[1] = 0; // how many transmissions failed before
    transmission[2] = _intsOnPinA;
    transmission[3] = _intsOnPinB;
    transmission[4] = transmissionNo++;
    transmission[5] = wdt_overruns;
    transmission[6] = magState; magState = 0;
}

void updateTransmission()
{
  if (interruptsOnPinA > transmission[2]) {
    transmission[2] = interruptsOnPinA;
  }
  if (interruptsOnPinB > transmission[3]) {
    transmission[3] = interruptsOnPinB;
  }
  transmission[5] = wdt_overruns;
  if (magState > transmission[6]) {
    transmission[6] = magState;
  }
}

float data[15][3];
int head = 0;
boolean initialized = false;

// 0 - delta is [0-5)
// 1 - delta is [5-10)
// 2 - delta is [10-15)
// 3 - delta is [15-20)
// 4 - delta is [20-30)
// 5 - delta is [30-40)
// 6 - delta is [40-60)
// 7 - delta is [60-100)
// 8 - delta is [100-?)
// 0xFF - not initialized yet
int getMagSensorState()
{
  /* Get a new sensor event */ 
  sensors_event_t event; 
  mag.getEvent(&event);

  data[head][0] = event.magnetic.x;
  data[head][1] = event.magnetic.y;
  data[head][2] = event.magnetic.z;
  
  head++;
  
  if (!initialized) {
    if (head == 15) {
      initialized = true;
    }
  }
  head = head % 15;

  if (initialized) {
    // now we can start algorithm
    float sumXTotal = 0;
    float sumYTotal = 0;
    float sumZTotal = 0;
    float sumXLastThree = 0;
    float sumYLastThree = 0;
    float sumZLastThree = 0;
    int pos = head;
    for (int i = 0; i < 15; i++) {
      sumXTotal += data[pos][0];
      sumYTotal += data[pos][1];
      sumZTotal += data[pos][2];
      if (i >= (15-3)) {
        sumXLastThree += data[pos][0];
        sumYLastThree += data[pos][1];
        sumZLastThree += data[pos][2];
      }
      pos = (pos + 1) % 15;
    }
    sumXTotal /= 15;
    sumYTotal /= 15;
    sumZTotal /= 15;
    sumXLastThree /= 3;
    sumYLastThree /= 3;
    sumZLastThree /= 3;

    float deltaX = sumXTotal - sumXLastThree; if (deltaX < 0) deltaX = -deltaX;
    float deltaY = sumYTotal - sumYLastThree; if (deltaY < 0) deltaY = -deltaY;
    float deltaZ = sumZTotal - sumZLastThree; if (deltaZ < 0) deltaZ = -deltaZ;
    float deltaSum = deltaX+deltaY+deltaZ;
    if (deltaSum < 5) {
      return 0;
    } else if (deltaSum < 10) {
      return 1;
    } else if (deltaSum < 15) {
      return 2;
    } else if (deltaSum < 20) {
      return 3;
    } else if (deltaSum < 30) {
      return 4;
    } else if (deltaSum < 40) {
      return 5;
    } else if (deltaSum < 60) {
      return 6;
    } else if (deltaSum < 100) {
      return 7;
    } else {
      return 8;
    }
  }
  return 0xFF;
}

void loop()
{
  /*Serial.print("W: ");
  Serial.println(f_wdt, DEC);
  Serial.flush();*/
  if (f_wdt == 0) {
    // not a watchdog interrupt!
    // so just put back to sleep
    enterSleep();
    
  } else if (f_wdt == 1) {
    // this is a watchdog timer interrupt!


    {
       // usefull load here
      byte _magState = getMagSensorState();
      if (_magState > magState) {
        magState = _magState;
      }
      
      if (lastTransmitFailed > 0 || transmitNowCounter == 6 /* 8sec * 7times = 56 seconds */) {
        putRadioUp();
        //
          if (lastTransmitFailed == 0) {
            initializeTransmission();
          } else {
            updateTransmission();
            transmission[1] = lastTransmitFailed; // how many transmissions failed before
          }
          wdt_reset();
          bool txSucceeded = radio.write(&transmission, sizeof(transmission));
          /*Serial.print(" TX: ");
          Serial.print(txSucceeded, DEC);
          Serial.print(" LFF: ");
          Serial.print(lastTransmitFailed, HEX);
          Serial.print(" TNC: ");
          Serial.println(transmitNowCounter, HEX);
          Serial.flush();*/
          wdt_reset();
        //
        putRadioDown();
        if (txSucceeded) {
          lastTransmitFailed = 0;
          transmitNowCounter = 0;
        } else {
          lastTransmitFailed++;
          if (lastTransmitFailed > 100) {
            lastTransmitFailed = 1; // avoid overload
          }
        }
      } else {
        transmitNowCounter++;
      }
    }
    
    wdt_reset();
    // put to sleep as normal
    f_wdt=0;
    enterSleep();
    
  } else {
    // this happens only when mcu hangs for some reason
    
    cli();
    while(true) {
      Serial.println("Program hanged!");
      Serial.flush();
    }
  }
}
