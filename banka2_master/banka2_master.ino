#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include "RF24.h"

const byte zoomerPin = 13;

ISR(WDT_vect)
{
  Serial.print("W");
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

  radio.openWritingPipe(addressSlave); // writing to Slave (Banka)
  radio.openReadingPipe(1, addressMaster);

  // Start the radio listening for data
  radio.startListening();
  
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
  while (!Serial) {}

  pinMode(zoomerPin, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(zoomerPin, HIGH);
    delay(1000);
    digitalWrite(zoomerPin, LOW);
    delay(1000);
  }
  
  setupWdt(true, WDT_PRSCL_8s);

  if (!setupRadio()) {
    while(true) {
      Serial.println("Radio Initialization failed!!!");
      for (int i = 0; i < 5; i++) {
        wdt_reset();
        digitalWrite(zoomerPin, HIGH);
        delay(300);
        wdt_reset();
        digitalWrite(zoomerPin, LOW);
        delay(300);
      }
      delay(2000);
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

byte transmission[8];

void loop()
{
  while (!radio.available()) {
  }
  radio.read( &transmission, sizeof(transmission) );
  Serial.println();
  Serial.print(" ID:"); Serial.print(transmission[0], HEX);// Banka(R) ID
  Serial.print(" C:");  Serial.print(transmission[1], HEX);// Communication No
  Serial.print(" F:");  Serial.print(transmission[2], HEX);// Failed attempts to deliver this communication
  Serial.print(" W:");  Serial.print(transmission[3], HEX);// WDT overruns
  Serial.print(" M:");  Serial.print(transmission[4], HEX);// Mag State
  Serial.print(" L:");  Serial.print(transmission[5], HEX);// Light State
  Serial.print(" A:");  Serial.print(transmission[6], HEX);// Port A interrupts
  Serial.print(" B:");  Serial.print(transmission[7], HEX);// Port B interrupts
  Serial.println();
}

