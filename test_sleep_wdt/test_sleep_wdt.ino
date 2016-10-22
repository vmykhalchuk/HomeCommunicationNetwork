#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>



const byte interruptPin = 2;
volatile byte f_wdt = 1;

void pinInterruptRoutine(void)
{
  /* We detach the interrupt to stop it from 
   * continuously firing while the interrupt pin
   * is low.
   */
  detachInterrupt(0);
  delay(100); // primitive debouncing
}

ISR(WDT_vect)
{
  //if(f_wdt == 0)
  //{
  //  f_wdt=1;
  //}
  //else
  //{
  //  Serial.println("WDT Overrun!!!");
  //}
}


/***************************************************
 *  Name:        enterSleep
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Enters the arduino into sleep mode.
 *
 ***************************************************/
void enterSleep(void)
{
  cli();
  // allowed modes:
  // SLEEP_MODE_IDLE         (0)
  // SLEEP_MODE_ADC          _BV(SM0)
  // SLEEP_MODE_PWR_DOWN     _BV(SM1)
  // SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
  // SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))
  // SLEEP_MODE_EXT_STANDBY  (_BV(SM0) | _BV(SM1) | _BV(SM2))

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // SLEEP_MODE_PWR_DOWN
  /* Setup pin2 as an interrupt and attach handler. */
  attachInterrupt(digitalPinToInterrupt(interruptPin), pinInterruptRoutine, LOW); // can be LOW / CHANGE / HIGH
  sleep_enable();
  sei();
  sleep_cpu(); /* The program will continue from here. */ // it was : sleep_mode() , seems to be same functionality
  sleep_disable();
}


/***************************************************
 *  Name:        setup
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Setup for the Arduino.           
 *
 ***************************************************/
void setup()
{
  Serial.begin(9600);
  
  /* Setup the pin direction. */
  pinMode(interruptPin, INPUT);

  if (false)
  {
    // disable BOD
    /*sbi(MCUCR,BODS);
    sbi(MCUCR,BODSE);
    sbi(MCUCR,BODS);
    cbi(MCUCR,BODSE);*/
  }

  if (false)
  {
    // disable ADC
    ADCSRA = 0;
    // in order to recover ADC after sleep - write ADCSRA to memory and restore it after wakeup
  }

  if (true) {
    /*** Setup the WDT ***/
    /* Clear the reset flag. */
    MCUSR &= ~(1<<WDRF);
    /* In order to change WDE or the prescaler, we need to
     * set WDCE (This will allow updates for 4 clock cycles).
     */
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    /* set new watchdog timeout prescaler value */
    // WDP0 & WDP3 = 8sec
    // WDIE = enable interrupt (note no reset)
    WDTCSR = 1<<WDIE | 1<<WDP0 | 1<<WDP3; /* 8.0 seconds */
  }
  
  Serial.println("Initialisation complete.");
}



/***************************************************
 *  Name:        loop
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Main application loop.
 *
 ***************************************************/
void loop()
{
  for (int seconds = 0; seconds < 3; seconds++)
  {
    delay(1000);
    Serial.print("Awake for ");
    Serial.print(seconds, DEC);
    Serial.println(" second");
    wdt_reset();
  }
  
  Serial.println("Entering sleep");
  delay(200);
  enterSleep();
}

