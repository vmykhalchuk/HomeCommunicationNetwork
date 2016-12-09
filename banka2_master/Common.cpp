#include "Arduino.h"
#include "Common.h"

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

RF24* radioObj;
volatile boolean isRadioUp = true;

bool setupRadio(RF24* radio)
{
  radioObj = radio;
  if (!radio->begin())
  {
    return false;
  }
  radio->stopListening();// stop to avoid reset MCU issues

  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio->setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio->setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio->setChannel(118); // 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio->setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  radio->openWritingPipe(addressSlave); // writing to Slave (Banka)
  radio->openReadingPipe(1, addressMaster);

  return true;
}

void putRadioDown()
{
  if (isRadioUp) {
    radioObj->powerDown();
  }
  isRadioUp = false;
}
void putRadioUp()
{
  if (!isRadioUp) {
    radioObj->powerUp();
  }
  isRadioUp = true;
}

