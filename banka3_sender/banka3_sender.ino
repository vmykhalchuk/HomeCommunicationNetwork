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
//#define SERIAL_DEBUG Serial
static const byte BANKA_DEV_ID = 0x11; // ID of this Banka(R)
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
#include <RF24.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

#include <VMUtils_ADC.h>
#include <VMUtils_WDT.h>

#define SERIAL_OUTPUT Serial
#include <VMUtils_Misc.h>

#include <HomeCommNetworkCommon.h>

#define _debug__dig(x) _debug(x)

const byte addressSlave[6] = {'S', 'B', 'a', '2', BANKA_DEV_ID};

/**
 * This system is waking up once a tick and performs actions as fast as possible to go sleep again.
 * Every tick is: 4 seconds
 */
static const byte TICK_SECONDS = 4;

Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345, HMC5883_MODE_SINGLE_MEASUREMENT);

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);

HomeCommNetworkCommon homeCommNetwork;

// >>>>> START_OF_DEBUG_SECTION (http://cpp.sh)
// transmission payload:
// 2m - 2m - 2m - 2m - 2m - 10m - 20m  - 40m - 80m - 160m - 320m - 640m - 1280m - 2560m - 5120m      <-- max 40 hours
//                                 2m     2m   10m    10m    20m    20m     40m     40m -   40m      <-- tollerance/resolution
// 4bit * 14 - light
// 4bit * 14 - magnetometr
// 4bit * 14 - 2 buttons + reset + reserved

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 128 of these ... - 40m

const uint8_t data_buf_size = 20+16+32+128;
uint8_t data_buf_1[data_buf_size]; // used by light
uint8_t data_buf_2[data_buf_size]; // used by magnetometer
uint8_t data_buf_3[data_buf_size]; // used by a&b inputs (0 - nothing triggered, 1 - a triggered, 2 - b triggered, 3 - a&b triggered)
void initDataBuf1and2()
{
  for (int i = 0; i < data_buf_size; i++)
  {
    data_buf_1[i] = 0;
    data_buf_2[i] = 0;
    data_buf_3[i] = 0;
  }
}

void storeOnPosition(uint8_t * transmitBufPointer, uint8_t pos, uint8_t data) {
  data &= 0x0f;
  if (pos % 2 == 0) {
    *(transmitBufPointer + pos/2) |= data; // lower bits
  } else {
    *(transmitBufPointer + pos/2) |= data << 4; // higher bits
  }
}

uint8_t calculate40mIntervals(uint8_t * data_buf_pointer, uint8_t number) {
  uint8_t res = 0;
  for (int i = 0; i < number; i++) {
    res = max(res, *(data_buf_pointer + 20 + 16 + 32 + i));
  }
  return res;
}

void loadToTransmitBuf(uint8_t * data_buf_pointer, uint8_t * transmitBufPointer /*8 bytes buffer*/)
{
  // clean transmit buf first
  for (int i = 0; i < 8; i++) {
    *(transmitBufPointer + i) = 0;
  }

  uint8_t pos = 0;
  // load first 5 2m intervals
  for (int i = 0; i < 5; i++) {
    storeOnPosition(transmitBufPointer, pos++, (*(data_buf_pointer + i)));
  }
  // load one 10m interval
  uint8_t value10m = (*(data_buf_pointer + 20));
  storeOnPosition(transmitBufPointer, pos++, value10m);
  // load one 20m interval
  uint8_t value20m = (*(data_buf_pointer + 20 + 16));
  storeOnPosition(transmitBufPointer, pos++, value20m);
  // load one 40m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 1));
  // calculate and load first 80m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 2));
  // calculate and load first 160m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 4));
  // calculate and load first 320m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 8));
  // calculate and load first 640m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 16));
  // calculate and load first 1280m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 32));
  // calculate and load first 2560m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 64));
  // calculate and load first 5120m interval
  storeOnPosition(transmitBufPointer, pos++, calculate40mIntervals(data_buf_pointer, 128));
}

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 128 of these ... - 40m
void __debugPrint2DecNumber(uint8_t z) {
  #ifdef SERIAL_DEBUG
    if (z >= 10) _debug__dig(z / 10) else _debug("0");
    _debug__dig(z % 10);
    _debug("-");
  #endif
}
void __debugStoredData(uint8_t * data_buf_pointer) {
  #ifdef SERIAL_DEBUG
                            _debug(" 2m[20]:  "); for (int i = 0; i < 20; i++) __debugPrint2DecNumber(*(data_buf_pointer+i)); _debugln();
    wdt_reset();
  _debug("              "); _debug("10m[16]:  "); for (int i = 0; i < 16; i++) __debugPrint2DecNumber(*(data_buf_pointer+20+i)); _debugln();
    wdt_reset();
  _debug("              "); _debug("20m[32]:  "); for (int i = 0; i < 16; i++) __debugPrint2DecNumber(*(data_buf_pointer+20+16+i)); _debugln();
    wdt_reset();
  _debug("              "); _debug("40m[128]: "); for (int i = 0; i < 16; i++) __debugPrint2DecNumber(*(data_buf_pointer+20+16+32+i)); _debugln();
    wdt_reset();
  #endif
}
void debugStoredData() {
  #ifdef SERIAL_DEBUG
    _debug("Buf_1(Light): "); __debugStoredData(&data_buf_1[0]);
    _debug("Buf_2(Magnt): "); __debugStoredData(&data_buf_2[0]);
    _debug("Buf_3( A_B ): "); __debugStoredData(&data_buf_3[0]);
  #endif
}

void debugPrintPos(uint8_t * pointer, int pos) {
  uint8_t z = *(pointer + pos/2);
  if (pos%2 == 0) {
    // read low bits
    z &= 0x0f;
  } else {
    // read high bits
    z = z >> 4;
  }

  #ifdef SERIAL_DEBUG
  _debug(" ");
  #endif
  __debugPrint2DecNumber(z);
}

void logResetSinceTime(uint16_t resetSinceMinutes)
{
  if (resetSinceMinutes == 0xFFFF) {
    _debug("more than 45 days");
    return;
  }
  uint16_t days = resetSinceMinutes / (24*60);
  uint16_t hours = resetSinceMinutes / 60;
  uint16_t minutesInLastHour = resetSinceMinutes % 60;

  _debug(days); _debug("d"); _debug(hours % 24); _debug("h"); _debug(minutesInLastHour); _debug("m");
}

void processRadioTransmission_debug(uint8_t* transmission)
{
  #ifdef SERIAL_DEBUG
    _debugln();
    _debug("Sending data: ");
    _debug(" ID:"); _debugF(transmission[0], HEX);// Banka(R) ID
    _debug(" V:");  _debugF(transmission[1]&0xF0 >> 4, HEX);// protocol version (1, 2, .. 0xF)
    _debug(" T:");  _debugF(transmission[1]&0x0F, HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
    wdt_reset();
    uint16_t bankaVccAdc = transmission[2]<<8|transmission[3];
    float bankaVcc = 1.1 / float (bankaVccAdc + 0.5) * 1024.0;
    _debug(" VCC:"); _debug__dig(bankaVcc);// Battery Voltage
    uint16_t resetSinceMinutes = transmission[4]<<8|transmission[5];
    _debug(" Rst:"); logResetSinceTime(resetSinceMinutes); // Reset since time
    _debugln();
    _debug("   Historical: - 2m- 2m- 2m- 2m- 2m-10m-20m-40m-80m-160-320-640-12 -25 -51 -"); _debugln();
    _debug("                                                     m   m   m  80m 60m 20m "); _debugln();
    wdt_reset();
    _debug("        Light: -"); for (int i = 0; i < 15; i++) debugPrintPos(&transmission[6], i); _debugln();
    wdt_reset();
    _debug("       Magnet: -"); for (int i = 0; i < 15; i++) debugPrintPos(&transmission[6+8], i); _debugln();
    wdt_reset();
    _debug("        Reset: -"); for (int i = 0; i < 15; i++) debugPrintPos(&transmission[6+16], i); _debugln();
    wdt_reset();
    _debugln();
  #endif
}

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 64 of these ... - 40m

void shift2mIntervals(uint8_t * data_buf_pointer) {
  for (int i = 19; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift10mIntervals(uint8_t * data_buf_pointer) {
  data_buf_pointer += 20;
  for (int i = 15; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift20mIntervals(uint8_t * data_buf_pointer) {
  data_buf_pointer += 20 + 16;
  for (int i = 31; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

void shift40mIntervals(uint8_t * data_buf_pointer) {
  data_buf_pointer += 20 + 16 + 32;
  for (int i = 127; i >= 1; i--) {
    *(data_buf_pointer + i) = *(data_buf_pointer + i - 1);
  }
}

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 64 of these ... - 40m
void shift2mIntervalAndRecalculate(uint8_t * data_buf_pointer, uint8_t newData, long secondsSinceStart)
{
  // shift 2m intervals to the right (first 2m interval will be set to newData)
  shift2mIntervals(data_buf_pointer);
  *(data_buf_pointer) = newData;

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 64 of these ... - 40m

  // update 10m intervals
  bool shift10m = secondsSinceStart % 600 == 0;
  if (shift10m) shift10mIntervals(data_buf_pointer);
  uint8_t offset2m_for_10m = 0;
  for (int i = 0; i < 4; i++) // recalculate first 4 10m intervals from 2m intervals
  {
    uint8_t val10m_i = *(data_buf_pointer + offset2m_for_10m);
    for (int j = 1; j < 5; j++)
    {
      val10m_i = max(val10m_i, *(data_buf_pointer + offset2m_for_10m + j));
    }
    *(data_buf_pointer + 20 + i) = val10m_i;
    offset2m_for_10m += 5;
  }

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 64 of these ... - 40m

  // update 20m intervals
  bool shift20m = secondsSinceStart % 1200 == 0;
  if (shift20m) shift20mIntervals(data_buf_pointer);
  uint8_t offset_10m_for_20m = 20;
  for (int i = 0; i < (shift10m ? 8 : 2); i++) // recalculate first 2 20m intervals from 4 first 10m intervals (8 20m intervals has to be recalculated if 10m intervals were shifted)
  {
    uint8_t val20m_i = *(data_buf_pointer + offset_10m_for_20m);
    for (int j = 1; j < 2; j++)
    {
      val20m_i = max(val20m_i, *(data_buf_pointer + offset_10m_for_20m + j));
    }
    *(data_buf_pointer + 20 + 16 + i) = val20m_i;
    offset_10m_for_20m += 2;
  }

  // update 40m intervals
  bool shift40m = secondsSinceStart % 2400 == 0;
  if (shift40m) shift40mIntervals(data_buf_pointer);
  uint8_t offset_20m_for_40m = 20 + 16;
  for (int i = 0; i < (shift20m ? 16 : 1); i++) // recalculate first 40m interval from 2 first 20m intervals (16 40m intervals has to be recalculated if 20m intervals were shifted)
  {
    uint8_t val40m_i = *(data_buf_pointer + offset_20m_for_40m);
    for (int j = 1; j < 2; j++)
    {
      val40m_i = max(val40m_i, *(data_buf_pointer + offset_20m_for_40m + j));
    }
    *(data_buf_pointer + 20 + 16 + 32 + i) = val40m_i;
    offset_20m_for_40m += 2;
  }
}

// 2m - 2m - ... 20 of these ... - 2m - 2m - 2m = 10m - 10m - ... 16 of these ... - 10m - 10m = 20m - 20m ... 32 of these ... - 20m = 40m - 40m - ... 128 of these ... - 40m

void updateRecent2mIntervalAndShiftWhenNeeded(uint8_t * data_buf_pointer, uint8_t newData, long secondsSinceStart)
{
  bool shift2m = secondsSinceStart % 120 == 0;
  if (shift2m)
  {
    // 2m interval, now we need to shift
    shift2mIntervalAndRecalculate(data_buf_pointer, newData, secondsSinceStart);
  }
  else
  {
    // update first 2m interval
    *data_buf_pointer = max(newData, *data_buf_pointer);
  
    // update first 10m interval
    uint8_t int10m_0 = *data_buf_pointer;
    for (int i = 1; i < 5; i++) {
      uint8_t int2m_i = *(data_buf_pointer + i);
      int10m_0 = max(int10m_0, int2m_i);
    }
    *(data_buf_pointer + 20) = int10m_0;
  
    // update first 20m interval
    uint8_t int20m_0 = max(*(data_buf_pointer + 20 + 0), *(data_buf_pointer + 20 + 1));
    *(data_buf_pointer + 20 + 16) = int20m_0;
  
    // update first 40m interval
    uint8_t int40m_0 = max(*(data_buf_pointer + 20 + 16 + 0), *(data_buf_pointer + 20 + 16 + 1));
    *(data_buf_pointer + 20 + 16 + 32) = int40m_0;
  }
}

long secondsSinceStart = 0;
// we must call it every 30seconds
void registerDataEvery30Seconds(uint8_t data1, uint8_t data2, uint8_t data3)
{
  #ifdef SERIAL_DEBUG
    _debug("> Storing Light: "); _debug__dig(data1);
    _debug("; Magnt: "); _debug__dig(data2);
    _debug(";   A_B: "); _debug__dig(data3);
    _debugln();
  #endif
  secondsSinceStart += 30;
  updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_1[0], data1, secondsSinceStart);
  updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_2[0], data2, secondsSinceStart);
  updateRecent2mIntervalAndShiftWhenNeeded(&data_buf_3[0], data3, secondsSinceStart);
  if (secondsSinceStart >= 2400 /*40m*/) {
    secondsSinceStart = 0;
  }
  debugStoredData();
}
// >>>>> END_OF_DEBUG_SECTION (http://cpp.sh)


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
  digitalWrite(LIGHT_SENSOR_PIN, HIGH); // enable pull up for light sensor
  
  VMUtils_WDT::setupWdt(true, VMUtils_WDT::PRSCL::_4s); // must conform to TICK_SECONDS

  if (!homeCommNetwork.setupRadio(&radio, false)) { // we disable AutoACK here to save battery
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
  //radio.openReadingPipe(1, homeCommNetwork.addressSlave);
  radio.openWritingPipe(homeCommNetwork.addressMaster);

  mag.begin();

  _println("Initialization complete.");
  for (int i = 0; i < 5; i++) {
    wdt_reset();
    digitalWrite(zoomerPin, HIGH);
    delay(200);
    wdt_reset();
    delay(300);
    wdt_reset();
    digitalWrite(zoomerPin, LOW);
    delay(200);
    wdt_reset();
    delay(300);
    wdt_reset();
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
// 6 - delta is [30-60)
// 7 - delta is [60-?)
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
    } else if (deltaSum < 60) {
      return 6;
    } else {
      return 7;
    }
  }
  return 0x0;
}
/// << mag sensor area ends
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


byte getLightSensorState()
{
  //digitalWrite(LIGHT_SENSOR_PIN, HIGH);
  int v = 1023 - constrain(analogRead(LIGHT_SENSOR_PIN), 0, 1023);
  //digitalWrite(LIGHT_SENSOR_PIN, LOW);
  return map(v, 0, 1023, 0, 1<<4 - 1);
}

uint16_t resetSinceMinutes = 0;
uint8_t resetSinceSeconds = 0;
void increaseResetSinceCounters() {
  resetSinceSeconds += TICK_SECONDS;
  if (resetSinceSeconds >= 60) {
    if (resetSinceMinutes < 0xFFFF) resetSinceMinutes++;
    resetSinceSeconds = resetSinceSeconds % 60;
  }
}

/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> transmission area start!

// type - 0 - normal transmission
//        1 - start-up transmission
//        2 - wdt overrun transmission
void _transmitData(byte type)
{
  homeCommNetwork.putRadioUp();
  wdt_reset();
  uint8_t transmission[32];
  // fill in data from structure
  transmission[0] = BANKA_DEV_ID;
  transmission[1] = type | 0x20; // 0x10 - version 1 of the protocol; 0x20 - version 2 ... etc; low digit is used for type
  transmission[2] = batteryVoltageHighByte;
  transmission[3] = batteryVoltageLowByte;
  transmission[4] = resetSinceMinutes >> 8;
  transmission[5] = resetSinceMinutes & 0x00FF;

  loadToTransmitBuf(&data_buf_1[0], &transmission[6]);   // 8 bytes for light
  wdt_reset();
  loadToTransmitBuf(&data_buf_2[0], &transmission[6+8]); // 8 bytes for magnetometer
  wdt_reset();
  loadToTransmitBuf(&data_buf_3[0], &transmission[6+16]);// 8 bytes for A_B

  processRadioTransmission_debug(&transmission[0]);
  wdt_reset();
  radio.write(&transmission, sizeof(transmission));
  wdt_reset();
  homeCommNetwork.putRadioDown();
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

uint8_t transmitMessageSeconds = 0;
uint8_t lastMagState, lastLightState, lasIntsOnPinA, lasIntsOnPinB;
bool cleanFlag = true;
void loop()
{
  if (f_wdt == 0) {
    // not a watchdog interrupt! this is pin interrupt!
    // so just put back to sleep
    enterSleep();
    
  } else if (f_wdt == 1) {
    f_wdt=0;

    // this is a watchdog timer interrupt!
    { // useful load here, is executed every TICK_SECONDS(2)
      #ifdef SERIAL_DEBUG
        digitalWrite(zoomerPin, HIGH);
        delay(10);
        digitalWrite(zoomerPin, LOW);
      #endif
      
      increaseResetSinceCounters();
      readBatteryLevel();


      {
        // we check sensors once 4 seconds
        uint8_t _magState = getMagSensorState();
        uint8_t _lightState = getLightSensorState();
        wdt_reset();
        uint8_t _intsOnPinA = readInterruptsOnPinA();
        uint8_t _intsOnPinB = readInterruptsOnPinB();
        wdt_reset();
        #ifdef SERIAL_DEBUG
          _debug("Data");
          _debug(" m="); _debug(_magState);
          _debug(" l="); _debug(_lightState);
          wdt_reset();
          _debug(" a="); _debug(_intsOnPinA);
          _debug(" b="); _debug(_intsOnPinB); _debugln();
          wdt_reset();
        #endif

        if (cleanFlag) {
          cleanFlag = false;
          lastMagState = 0;
          lastLightState = 0;
          lasIntsOnPinA = 0;
          lasIntsOnPinB = 0;
        }
        lastMagState = max(lastMagState, _magState);
        lastLightState = max(lastLightState, _lightState);
        lasIntsOnPinA = max(lasIntsOnPinA, _intsOnPinA);
        lasIntsOnPinB = max(lasIntsOnPinB, _intsOnPinB);
        wdt_reset();
      }
      
      transmitMessageSeconds += TICK_SECONDS;
      if (transmitMessageSeconds >= 30) { // every 30 seconds we call registerData and attempt transmission
        registerDataEvery30Seconds(lastLightState, lastMagState, lasIntsOnPinA);
        _transmitData(0);
        transmitMessageSeconds = transmitMessageSeconds % 30;
        cleanFlag = true;
      }

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
      for (int i = 0; i < 50; i++) {
        if (i % 10 == 0) {
          _transmitData(3);
        }
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

