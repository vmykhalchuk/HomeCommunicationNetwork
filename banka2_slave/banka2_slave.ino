/*
 * Connect Mag Sensor:
 * - connect to 5V input (not to 3.3V input) to avoid damage, it has own 3.3v low drain current stabilizer
 * - SDA => A4 => pin 27
 * - SCL => A5 => pin 28
 * 
 * Radio module:
 * - 
 * 
 * Light sensor:
 * - A3 => pin 26
 * 
 * Digital sensors on interrupt:
 * - sensor A => D2 => pin 4
 * - sensor B => D3 => pin 5
 */

/**
 * Board ID 0x12 pinout:
 * 
 *  | GND blue   <=> GND |TTL
 * B| RXD brown  <=> TXD |RS-232
 * O| TXD red    <=> RXD |(USB)
 * A|
 * R| GND brown <=> "-" buzzer
 * D|  D5 red   <=> "+" buzzer
 *  |
 */

/**
 * DEFINE BEFORE UPLOAD :: START
 */
#define SERIAL_DEBUG Serial
static const byte BANKA_NO = 1; // from 1 to 5
const byte zoomerPin = 5; // cannot be 13 (Arduino LED) since radio is using it!
const byte INTERRUPT_PIN_A = 2;
const byte INTERRUPT_PIN_B = 3;
const byte LIGHT_SENSOR_PIN = A3; // (pin #26 of ATMega328P)
/**
 * DEFINE BEFORE UPLOAD :: END
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

#include <VMUtils_ADC.h>
#include <VMUtils_WDT.h>

#define SERIAL_OUTPUT Serial
#include <VMUtils_Misc.h>

#include <HomeCommNetworkCommon.h>

static const byte BANKA_DEV_ID = (byte) (rAddress[BANKA_NO] & 0xFF); // ID of this Banka(R)
//const byte addressSlave[6] = {'S', 'B', 'a', 'n', BANKA_DEV_ID};

/**
 * This system is waking up once a tick and performs actions as fast as possible to go sleep again.
 * Every tick is: 4 seconds
 */
static const byte TICK_SECONDS = 4;

static const byte SEND_TRANSMISSION_TRESHOLD = 48/TICK_SECONDS; // every tick is every TICK_SECONDS(4) seconds,
                                                  // then TICK_SECONDS*12 = 48 seconds between transmissions

Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);

uint8_t batteryVoltageLowByte = 0, batteryVoltageHighByte = 0;
void batteryVoltageRead()
{
  uint16_t vcc = VMUtils_ADC::readVccAsUint();
  batteryVoltageHighByte = vcc>>8;
  batteryVoltageLowByte = vcc&0x00FF;
  #ifdef SERIAL_DEBUG
    float r = 1.1 / float (vcc + 0.5) * 1024.0;
    _debug("vcc: "); _debugln(r);
  #endif
}

/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> interrupts routines start

#include "Misc.h"

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

const uint64_t PTXpipe = rAddress[BANKA_NO];

void setup()
{
  Serial.begin(57600);
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
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  digitalWrite(INTERRUPT_PIN_A, HIGH);
  digitalWrite(INTERRUPT_PIN_B, HIGH);
  digitalWrite(LIGHT_SENSOR_PIN, HIGH); // pull it up for light sensor
  
  VMUtils_WDT::setupWdt(true, VMUtils_WDT::PRSCL::_4s); // must conform to TICK_SECONDS

  if (!setupRadio(radio)) {
    while(true) {
      _println("Radio Initialization failed!!!");
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
  radio.openReadingPipe(0,PTXpipe);  //open reading or receive pipe
  radio.stopListening(); //go into transmit mode
  radio.openWritingPipe(PTXpipe);        //open writing or transmit pipe
  //radio.openReadingPipe(1, addressSlave);
  //radio.openWritingPipe(addressMaster);

  mag.begin();

  _println("Initialization complete.");
  for (int i = 0; i < 5; i++) {
    wdt_reset();
    digitalWrite(zoomerPin, HIGH);
    delay(500);
    wdt_reset();
    digitalWrite(zoomerPin, LOW);
    delay(500);
  }

  // read battery into local variables
  batteryVoltageRead();
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


byte getLightSensorState()
{
  int v = 1023 - constrain(analogRead(LIGHT_SENSOR_PIN), 0, 1023);
  return map(v, 0, 1023, 0, 255);
}


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
      _debugln("Transmit succeeded");
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
      _debug("Transmit failed: "); _debugln(_rneD.lastFailed);
      _rneD.lastFailed++; if (_rneD.lastFailed > 100) _rneD.lastFailed = 100;
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = _getTreshold(_rneD.lastFailed);
    }
  }
}

byte _getTreshold(byte lastFailed)
{
  if (lastFailed > 9) lastFailed = 9; // result is 34 when lastFailed=9 (meaning 34*TICK_SECONDS(4) = 2.26 minutes to next retry)
  byte v1 = 0, v2 = 1;
  for (int i = 0; i < lastFailed; i++)
  {
    byte s = v1;
    v1 = v2;
    v2 += s;
  }
  return min(v2, SEND_TRANSMISSION_TRESHOLD<<1); // wait time between failed retries should not be longer then twice as usual sending wait time
}

// type - 0 - normal transmission
//        1 - start-up transmission
//        2 - wdt overrun transmission
bool _transmitData(byte type)
{
  putRadioUp(radio);
  wdt_reset();
  byte transmission[32];
  // fill in data from structure
  transmission[0] = BANKA_DEV_ID;
  transmission[1] = type | 0x10; // 0x10 - version 1 of the protocol; 0x20 - version 2 ... etc; low digit is used for type
  transmission[2] = _rneD.transmissionId;
  transmission[3] = _rneD.lastFailed;
  transmission[4] = _rneD.wdtOverruns;
  transmission[5] = _rneD.magSensorData;
  transmission[6] = _rneD.lightSensorData;
  transmission[7] = _rneD.portAData;
  transmission[8] = _rneD.portBData;
  transmission[9] = batteryVoltageHighByte;
  transmission[10] = batteryVoltageLowByte;
  for (byte i = 11; i < 32; i++)
  {
    transmission[i] = 0;
  }
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
  putRadioDown(radio);
  return txSucceeded;
}

/// << transmission area end!
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

int readBatteryLevelCounter = 0;
void readBatteryLevel() // is called every TICK_SECONDS(4) seconds
{
  if (readBatteryLevelCounter++ > ((60/TICK_SECONDS)*12)) // read battery measurement every 12 minutes
  {
    readBatteryLevelCounter = 0;
    batteryVoltageRead();
  }
}

void loop()
{
  if (f_wdt == 0) {
    // not a watchdog interrupt! this is pin interrupt!
    // so just put back to sleep
    enterSleep();
    
  } else if (f_wdt == 1) {
    f_wdt=0;
    
    // this is a watchdog timer interrupt!
    { // useful load here, is executed every TICK_SECONDS(4)
      #ifdef SERIAL_DEBUG
        digitalWrite(zoomerPin, HIGH);
        delay(10);
        digitalWrite(zoomerPin, LOW);
      #endif
      
      readBatteryLevel();
      
      _debug("Data");
      byte _magState = getMagSensorState();
      _debug(" m="); _debug(_magState);
      wdt_reset();
      byte _lightState = getLightSensorState();
      _debug(" l="); _debug(_lightState);
      wdt_reset();
      byte _intsOnPinA = readInterruptsOnPinA();
      _debug(" a="); _debug(_intsOnPinA);
      wdt_reset();
      byte _intsOnPinB = readInterruptsOnPinB();
      _debug(" b="); _debug(_intsOnPinB); _debugln();
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
      _println("Program hanged!");
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

