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
//#define DISABLE_MAG_SENSOR 1
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
#include <VProtocolSenderHelper.h>

#include "MiscHelper.h"
#include "Misc.h"


const uint64_t PTXpipe = rAddress[BANKA_NO]; // see in HomeCommNetworkCommon
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

#ifdef DISABLE_MAG_SENSOR
  MiscHelper miscHelper(&mag, LIGHT_SENSOR_PIN, false);
#else
  MiscHelper miscHelper(&mag, LIGHT_SENSOR_PIN, true);
#endif

uint16_t batteryVoltage = 0;
void batteryVoltageRead()
{
  batteryVoltage = VMUtils_ADC::readVccAsUint();
  #ifdef SERIAL_DEBUG
    float r = 1.1 / float (batteryVoltage + 0.5) * 1024.0;
    _debug("vcc: "); _debugln(r);
  #endif
}
int readBatteryLevelCounter = 0;
void readBatteryLevel() // is called every TICK_SECONDS(4) seconds
{
  if (readBatteryLevelCounter++ > ((60/TICK_SECONDS)*12)) // read battery measurement every 12 minutes
  {
    readBatteryLevelCounter = 0;
    batteryVoltageRead();
  }
}

/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> interrupts routines start

byte _ints_interruptsOnPinA = 0;
byte _ints_interruptsOnPinB = 0;
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
  }
}

/***************************************
 *  VProtocolSenderHelper * START *
 ***************************************/

class VProtocol_SenderHelperCallback_Impl : public VProtocol_SenderHelperCallback
{
  public:
    void latchData(uint8_t* dataBuf);
    void peekData(uint8_t* dataBuf);
    void peekNonTransactionalData(uint8_t* dataBuf);

    void registerWdtOverruns(uint8_t wdtOverruns);
    void registerMagSensorState(uint8_t magSensorState);
    void registerLightSensorState(uint8_t lightSensorState);
    void registerPortAInterrupts(uint8_t portAInterrupts);
    void registerPortBInterrupts(uint8_t portBInterrupts);
  private:
    uint8_t wdtOverruns = 0;
    uint8_t magSensorMaxValue = 0;
    uint8_t lightSensorMaxValue = 0;
    uint16_t lightSensorSumValue = 0;
    uint8_t lightSensorValuesSummed = 0;
    uint8_t portAData = 0;
    uint8_t portBData = 0;
};

void VProtocol_SenderHelperCallback_Impl::registerWdtOverruns(uint8_t wdtOverruns) {
  this->wdtOverruns = max(this->wdtOverruns, wdtOverruns);
}
void VProtocol_SenderHelperCallback_Impl::registerMagSensorState(uint8_t magSensorState) {
  this->magSensorMaxValue = max(this->magSensorMaxValue, magSensorState);
}
void VProtocol_SenderHelperCallback_Impl::registerLightSensorState(uint8_t lightSensorState) {
  this->lightSensorMaxValue = max(this->lightSensorMaxValue, lightSensorState);
  if (this->lightSensorValuesSummed < 250) {
    this->lightSensorValuesSummed++;
    this->lightSensorSumValue += lightSensorState;
  }
}
void VProtocol_SenderHelperCallback_Impl::registerPortAInterrupts(uint8_t portAInterrupts) {
  this->portAData = max(this->portAData, portAInterrupts);
}
void VProtocol_SenderHelperCallback_Impl::registerPortBInterrupts(uint8_t portBInterrupts) {
  this->portBData = max(this->portBData, portBInterrupts);
}

void VProtocol_SenderHelperCallback_Impl::latchData(uint8_t* dataBuf) {
  this->peekData(dataBuf);
  this->wdtOverruns = 0;
  this->magSensorMaxValue = 0;
  this->lightSensorMaxValue = 0;
  this->portAData = 0;
  this->portBData = 0;
}

void VProtocol_SenderHelperCallback_Impl::peekData(uint8_t* dataBuf) {
  *(dataBuf+0) = this->wdtOverruns;
  *(dataBuf+1) = this->magSensorMaxValue;
  *(dataBuf+2) = this->lightSensorMaxValue;
  *(dataBuf+3) = this->portAData;
  *(dataBuf+4) = this->portBData;

  uint8_t lightSensorAverageValue = 0;
  if (this->lightSensorValuesSummed > 0) {
    lightSensorAverageValue = this->lightSensorSumValue / this->lightSensorValuesSummed;
  }
  *(dataBuf+5) = lightSensorAverageValue;
}

void VProtocol_SenderHelperCallback_Impl::peekNonTransactionalData(uint8_t* dataBuf)
{
  *(dataBuf+0) = BANKA_DEV_ID;
  *(dataBuf+1) = batteryVoltage>>8;// high byte
  *(dataBuf+2) = batteryVoltage&0x00FF; // low byte
}

class VProtocol_DataSender_Impl : public VProtocol_DataSender
{
  public:
    void sendData(uint8_t* data, bool& res);
};

void VProtocol_DataSender_Impl::sendData(uint8_t* data, bool& res)
{
  putRadioUp(radio);
  wdt_reset();
  res = radio.write(data, 32);
  wdt_reset();
  putRadioDown(radio);
}

VProtocol_SenderHelperCallback_Impl vProtocolSenderHelperCallbackImpl;
VProtocol_DataSender_Impl vProtocolDataSenderImpl;
VProtocolSenderHelper senderHelper(&vProtocolDataSenderImpl, &vProtocolSenderHelperCallbackImpl);

/***************************************
 *  VProtocolSenderHelper * END *
 ***************************************/



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
  //digitalWrite(LIGHT_SENSOR_PIN, HIGH); // pull it up for light sensor
  // Energy saving tip: to save energy while sleeping, we might disable pullup for light sensor. Also we might use external pullup for interrupt pins to reduce current since internal resistors are pretty low value.
  
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
/// >> transmission area start!
struct RegisterNewEventData
{
  byte sendTransmissionCounter = SEND_TRANSMISSION_TRESHOLD; // send after given amount of register event calls
  byte sendTransmissionTreshold = SEND_TRANSMISSION_TRESHOLD;
  byte lastFailed = 0;
} _rneD;

void registerNewEvent()
{
  if (++_rneD.sendTransmissionCounter >= _rneD.sendTransmissionTreshold)
  {
    // send transmission with data in _rneD
    bool succeeded = senderHelper.sendPacket();
    if (succeeded)
    {
      _debugln("Transmit succeeded");
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = SEND_TRANSMISSION_TRESHOLD;
      _rneD.lastFailed = 0;
    }
    else
    {
      _debugln("Transmit failed!");
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = _getTreshold(_rneD.lastFailed);
      _rneD.lastFailed++;
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
    { // useful load here, is executed every TICK_SECONDS(4)
      #ifdef SERIAL_DEBUG
        digitalWrite(zoomerPin, HIGH);
        delay(10);
        digitalWrite(zoomerPin, LOW);
      #endif
      
      readBatteryLevel();

      _debug("Data");
      byte _magState = miscHelper.getMagSensorState();
      _debug(" m="); _debug(_magState);
      wdt_reset();
      byte _lightState = miscHelper.getLightSensorState();
      _debug(" l="); _debug(_lightState);
      wdt_reset();
      byte _intsOnPinA = readInterruptsOnPinA();
      _debug(" a="); _debug(_intsOnPinA);
      wdt_reset();
      byte _intsOnPinB = readInterruptsOnPinB();
      _debug(" b="); _debug(_intsOnPinB); _debugln();
      wdt_reset();

      vProtocolSenderHelperCallbackImpl.registerWdtOverruns(wdt_overruns);
      vProtocolSenderHelperCallbackImpl.registerMagSensorState(_magState);
      vProtocolSenderHelperCallbackImpl.registerLightSensorState(_lightState);
      vProtocolSenderHelperCallbackImpl.registerPortAInterrupts(_intsOnPinA);
      vProtocolSenderHelperCallbackImpl.registerPortBInterrupts(_intsOnPinB);
      registerNewEvent();
    }
    
    wdt_reset();
    #ifdef SERIAL_DEBUG
      Serial.flush(); // flush before going sleep
    #endif
    // put to sleep as normal
    enterSleep();
    
  } else {
    // this happens only when mcu hangs for some reason
    wdt_overruns++;
    
    cli();
    while(true) {
      _println("Program hanged!");
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

