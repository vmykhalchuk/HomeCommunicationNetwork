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
static const byte SEND_TRANSMISSION_TRESHOLD = 6; // every tick is every 8 seconds,
                                                  // then 8*6 = 48 seconds between transmissions

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
  /*if (f_wdt == 0)
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
  }*/
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
  pinMode(INTERRUPT_PIN_A, INPUT);
  pinMode(INTERRUPT_PIN_B, INPUT);

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

  mag.begin();

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


/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// >> mag sensor area starts
float _mss_data[15][3];
int _mss_head = 0;
boolean _mss_initialized = false;
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
struct RegsiterNewEventData
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

  if (++_rneD.sendTransmissionCounter >= _rneD.sendTransmissionTreshold)
  {
    // send transmission with data in _rneD
    boolean succeeded = _transmitData();
    if (succeeded)
    {
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
  byte v1 = 0, v2 = 1;
  for (int i = 0; i < lastFailed; i++)
  {
    byte s = v1;
    v1 = v2;
    v2 = s + v2;
  }
  return v1;
}

boolean _transmitData()
{
  putRadioUp();
  wdt_reset();
  byte transmission[8];
  // fill in data from structure
  transmission[0] = BANKA_DEV_ID;
  transmission[1] = _rneD.transmissionId;
  transmission[2] = _rneD.lastFailed;
  transmission[3] = _rneD.wdtOverruns;
  transmission[4] = _rneD.magSensorData;
  transmission[5] = _rneD.lightSensorData;
  transmission[6] = _rneD.portAData;
  transmission[7] = _rneD.portBData;
  bool txSucceeded = radio.write(&transmission, sizeof(transmission));
  wdt_reset();
  putRadioDown();
  return txSucceeded;
}
/// << transmission area end!
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void loop()
{
  /*Serial.print("W: ");
  Serial.println(f_wdt, DEC);
  Serial.flush();*/
  if (f_wdt == 0) {
    // not a watchdog interrupt! this is pin interrupt!
    // so just put back to sleep
    enterSleep();
    
  } else if (f_wdt == 1) {
    f_wdt=0;
    // this is a watchdog timer interrupt!
    { // useful load here
      byte _magState = getMagSensorState();
      wdt_reset();
      byte _lightState = getLightSensorState();
      wdt_reset();
      byte _intsOnPinA = readInterruptsOnPinA();
      wdt_reset();
      byte _intsOnPinB = readInterruptsOnPinB();
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

