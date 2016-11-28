/*
 * Connect Mag Sensor:
 * - connect to 5V (not to 3.3V) in to avoid damage - it works even with lower voltage then 3.3, so no problem
 * - SDA => A4 => pin 27
 * - SCL => A5 => pin 28
 * 
 * Radio module:
 * - 
 * 
 * Light sensor:
 * - A3 => pin 26 (could be to A0 / pin 23)
 * 
 * Digital sensors on interrupt:
 * - sensor A => D2 => pin 4
 * - sensor B => D3 => pin 5
 */

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include "RF24.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

static const byte BANKA_DEV_ID = 0x11; // ID of this Banka(R)
static const byte SEND_TRANSMISSION_TRESHOLD = 12; // every tick is every 8 seconds,
                                                  // then 4*12 = 48 seconds between transmissions

const byte zoomerPin = 5; // cannot be 13 since radio is using it!

const byte INTERRUPT_PIN_A = 2;
const byte INTERRUPT_PIN_B = 3;

const byte LIGHT_SENSOR_PIN = A3;

/* Assign a unique ID to this sensor at the same time */
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);


/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> interrupts routines start
volatile byte _ints_interruptsOnPinA = 0;
volatile byte _ints_interruptsOnPinB = 0;
void pinAInterruptRoutine(void)
{
  if (_ints_interruptsOnPinA <= 0xD) {
    _ints_interruptsOnPinA++;
  } else {
    _ints_interruptsOnPinA = 0xF;
    detachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN_A));
  }
}

void pinBInterruptRoutine(void)
{
  if (_ints_interruptsOnPinB <= 0xD) {
    _ints_interruptsOnPinB++;
  } else {
    _ints_interruptsOnPinB = 0xF;
    detachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN_B));
  }
}

byte readInterruptsOnPinA()
{
  cli();
  byte _intsOnPinA = _ints_interruptsOnPinA;
  _ints_interruptsOnPinA = 0;
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN_A), pinAInterruptRoutine, LOW);
  sei();
  return _intsOnPinA;
}

byte readInterruptsOnPinB()
{
  cli();
  byte _intsOnPinB = _ints_interruptsOnPinB;
  _ints_interruptsOnPinB = 0;
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN_B), pinBInterruptRoutine, LOW);
  sei();
  return _intsOnPinB;
}
/// << interrupts routines end
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


volatile byte f_wdt=1;
volatile byte wdt_overruns = 0;
ISR(WDT_vect)
{
  f_wdt++;
  if (f_wdt>100) f_wdt=6;
  if ((f_wdt > 5) && (f_wdt%10 == 0)) {
    for (int i = 0; i < 50; i++) {
      wdt_reset();
      digitalWrite(zoomerPin, HIGH);
      delay(300);
      wdt_reset();
      digitalWrite(zoomerPin, LOW);
      delay(300);
    }
    if (f_wdt%20 == 0) {
      _transmitData(2);
    }
  }
}

inline void enterSleepAndAttachInterrupt(void)
{
  cli();
  /* Setup pin2 as an interrupt and attach handler. */
            // digitalPinToInterrupt(2) => 0
            // digitalPinToInterrupt(3) => 1
            // digitalPinToInterrupt(1 | 4) => -1
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN_A), pinAInterruptRoutine, LOW); // can be LOW / CHANGE / HIGH / ... FALLING
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN_B), pinBInterruptRoutine, LOW);

  enterSleep();
}

inline void enterSleep(void)
{
  byte adcsra = ADCSRA;          //save the ADC Control and Status Register A
  ADCSRA = 0;                    //disable the ADC
  // allowed modes:
  // SLEEP_MODE_IDLE         (0)
  // SLEEP_MODE_ADC          _BV(SM0)
  // SLEEP_MODE_PWR_DOWN     _BV(SM1)
  // SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
  // SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))
  // SLEEP_MODE_EXT_STANDBY  (_BV(SM0) | _BV(SM1) | _BV(SM2))
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  cli();
  sleep_enable();
  //disable brown-out detection while sleeping (20-25µA)
  uint8_t mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);
  uint8_t mcucr2 = mcucr1 & ~_BV(BODSE);
  MCUCR = mcucr1;
  MCUCR = mcucr2;
  sei(); //after sei we have one clock cycle to execute any command before interrupt will be activated
  sleep_cpu(); /* The program will continue from here. */ // it was : sleep_mode() , seems to be same functionality
  sleep_disable();
  ADCSRA = adcsra;               //restore ADCSRA
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
  while(!Serial);

  pinMode(zoomerPin, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(zoomerPin, HIGH);
    delay(1000);
    digitalWrite(zoomerPin, LOW);
    delay(1000);
  }
  
  /* Setup the pin directions */
  pinMode(INTERRUPT_PIN_A, INPUT);
  pinMode(INTERRUPT_PIN_B, INPUT);

  setupWdt(true, WDT_PRSCL_4s);

  if (!setupRadio()) {
    while(true) {
      Serial.println("Radio Initialization failed!!!");
      Serial.flush();
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

  mag.begin();

  Serial.println("Initialization complete.");
  Serial.flush();
  for (int i = 0; i < 5; i++) {
    wdt_reset();
    digitalWrite(zoomerPin, HIGH);
    delay(500);
    wdt_reset();
    digitalWrite(zoomerPin, LOW);
    delay(500);
  }
  
}


/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> mag sensor area starts
volatile float _mss_data[15][3];
volatile int _mss_head = 0;
volatile boolean _mss_initialized = false;
// 1 - delta is [0-5)
// 2 - delta is [5-10)
// 3 - delta is [10-15)
// 4 - delta is [15-20)
// 5 - delta is [20-30)
// 6 - delta is [30-40)
// 7 - delta is [40-60)
// 8 - delta is [60-100)
// 9 - delta is [100-?)
// 0 - not initialized yet
byte getMagSensorState()
{
  /* Get a new sensor event */ 
  sensors_event_t event; 
  mag.getEvent(&event);

  _mss_data[_mss_head][0] = event.magnetic.x;
  _mss_data[_mss_head][1] = event.magnetic.y;
  _mss_data[_mss_head][2] = event.magnetic.z;
  
  _mss_head++;
  
  if (!_mss_initialized) {
    if (_mss_head == 15) {
      _mss_initialized = true;
    }
  }
  _mss_head = _mss_head % 15;

  if (_mss_initialized) {
    // now we can start algorithm
    float sumXTotal = 0;
    float sumYTotal = 0;
    float sumZTotal = 0;
    float sumXLastThree = 0;
    float sumYLastThree = 0;
    float sumZLastThree = 0;
    int pos = _mss_head;
    for (int i = 0; i < 15; i++) {
      sumXTotal += _mss_data[pos][0];
      sumYTotal += _mss_data[pos][1];
      sumZTotal += _mss_data[pos][2];
      if (i >= (15-3)) {
        sumXLastThree += _mss_data[pos][0];
        sumYLastThree += _mss_data[pos][1];
        sumZLastThree += _mss_data[pos][2];
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
      return 1;
    } else if (deltaSum < 10) {
      return 2;
    } else if (deltaSum < 15) {
      return 3;
    } else if (deltaSum < 20) {
      return 4;
    } else if (deltaSum < 30) {
      return 5;
    } else if (deltaSum < 40) {
      return 6;
    } else if (deltaSum < 60) {
      return 7;
    } else if (deltaSum < 100) {
      return 8;
    } else {
      return 9;
    }
  }
  return 0x0;
}
/// << mag sensor area ends
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> light sensor area starts
byte getLightSensorState()
{
  int v = analogRead(LIGHT_SENSOR_PIN);
  for (int i = 0; i < 10; i++) {
    if (v < 50 + i*100) {
      return i;
    }
  }
  return 10;
}
/// << light sensor area ends
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> transmission area start!
struct RegisterNewEventData
{
  byte transmissionId = 0;// just running number from 0 to 255 and back to 0
  byte lastFailed = 0;
  byte sendTransmissionCounter = 0; // send after given amount of register event calls
  byte sendTransmissionTreshold = SEND_TRANSMISSION_TRESHOLD;

  byte wdtOverruns = 0;
  byte magSensorData = 0;
  byte lightSensorData = 0;
  byte portAData = 0;
  byte portBData = 0;
} _rneD;

volatile bool mcuInit = true;

void registerNewEvent(byte wdtOverruns, 
                      byte magSensorData, byte lightSensorData,
                      byte portAData, byte portBData)
{
  // update current values with most recent ones
  if (wdtOverruns > _rneD.wdtOverruns)
          _rneD.wdtOverruns = wdtOverruns;
  if (magSensorData > _rneD.magSensorData) 
          _rneD.magSensorData = magSensorData;
  if (lightSensorData > _rneD.lightSensorData) 
          _rneD.lightSensorData = lightSensorData;
  if (portAData > _rneD.portAData) 
          _rneD.portAData = portAData;
  if (portBData > _rneD.portBData) 
          _rneD.portBData = portBData;

  if (mcuInit || (++_rneD.sendTransmissionCounter >= _rneD.sendTransmissionTreshold))
  {
    // send transmission with data in _rneD
    boolean succeeded = _transmitData(mcuInit ? 1 : 0);
    if (succeeded)
    {
      if (mcuInit) mcuInit = false;
      
      _rneD.transmissionId++;
      _rneD.lastFailed = 0;
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = SEND_TRANSMISSION_TRESHOLD;
      _rneD.wdtOverruns = 0;
      _rneD.magSensorData = 0;
      _rneD.lightSensorData = 0;
      _rneD.portAData = 0;
      _rneD.portBData = 0;
    }
    else
    {
      _rneD.lastFailed++; if (_rneD.lastFailed > 100) _rneD.lastFailed = 100;
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = _getTreshold(_rneD.lastFailed);
    }
  }
}

byte _getTreshold(byte lastFailed)
{
  if (lastFailed > 9) lastFailed = 9; // result is 34 when lastFailed=9
  byte v1 = 0, v2 = 1;
  for (int i = 0; i < lastFailed; i++)
  {
    byte s = v1;
    v1 = v2;
    v2 = s + v2;
  }
  return v1;
}

// type - 0 - normal transmission
//        1 - start-up transmission
//        2 - wdt overrun transmission
bool _transmitData(byte type)
{
  putRadioUp();
  wdt_reset();
  byte transmission[9];
  // fill in data from structure
  transmission[0] = BANKA_DEV_ID;
  transmission[1] = type;
  transmission[2] = _rneD.transmissionId;
  transmission[3] = _rneD.lastFailed;
  transmission[4] = _rneD.wdtOverruns;
  transmission[5] = _rneD.magSensorData;
  transmission[6] = _rneD.lightSensorData;
  transmission[7] = _rneD.portAData;
  transmission[8] = _rneD.portBData;
  bool txSucceeded = radio.write(&transmission, sizeof(transmission));
  wdt_reset();
  { // notify of send status
    if (txSucceeded) {
      digitalWrite(zoomerPin, HIGH);
      delay(100);
      digitalWrite(zoomerPin, LOW);
      wdt_reset();
    } else {
      for (int i = 0; i < 10; i++) {
        wdt_reset();
        digitalWrite(zoomerPin, HIGH);
        delay(15);
        wdt_reset();
        digitalWrite(zoomerPin, LOW);
        delay(15);
      }
    }
  }
  putRadioDown();
  return txSucceeded;
}

/// << transmission area end!
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void loop()
{
  if (f_wdt == 0) {
    // not a watchdog interrupt! this is pin interrupt!
    // so just put back to sleep
    enterSleep();
    
  } else if (f_wdt == 1) {
    f_wdt=0;
    
    // this is a watchdog timer interrupt!
    { // useful load here
      digitalWrite(zoomerPin, HIGH);
      delay(10);
      digitalWrite(zoomerPin, LOW);
      
      //Serial.print("Data"); Serial.flush();
      byte _magState = getMagSensorState();
      //Serial.print(" m="); Serial.print(_magState); Serial.flush();
      wdt_reset();
      byte _lightState = getLightSensorState();
      //Serial.print(" l="); Serial.print(_lightState); Serial.flush();
      wdt_reset();
      byte _intsOnPinA = readInterruptsOnPinA();
      //Serial.print(" a="); Serial.print(_intsOnPinA); Serial.flush();
      wdt_reset();
      byte _intsOnPinB = readInterruptsOnPinB();
      //Serial.print(" b="); Serial.print(_intsOnPinB); Serial.println(); Serial.flush();
      wdt_reset();
      registerNewEvent(wdt_overruns, _magState, _lightState, _intsOnPinA, _intsOnPinB);
    }
    
    wdt_reset();
    // put to sleep as normal
    enterSleep();
    
  } else {
    // this happens only when mcu hangs for some reason
    wdt_overruns++;
    
    cli();
    while(true) {
      Serial.println("Program hanged!");
      Serial.flush();
      _transmitData(3);
      for (int i = 0; i < 50; i++) {
        wdt_reset();
        digitalWrite(zoomerPin, HIGH);
        delay(300);
        wdt_reset();
        digitalWrite(zoomerPin, LOW);
        delay(300);
      }
    }
    sei();
  }
}

