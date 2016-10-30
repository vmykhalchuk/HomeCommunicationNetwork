#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

const byte interruptPin0 = 2;
const byte interruptPin1 = 3;
volatile byte interruptsOnPin0 = 0;
const byte zoomerPin = 13;

void pin0InterruptRoutine(void)
{
  if (interruptsOnPin0 <= 0xD) {
    interruptsOnPin0++;
  } else {
    interruptsOnPin0 = 0xF;
    /* We detach the interrupt to stop it from 
     * continuously firing while the interrupt pin
     * is low.
     */
    detachInterrupt(digitalPinToInterrupt(interruptPin0));
  }
}

volatile byte f_wdt=1;
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
    Serial.println("WDT Overrun!!!");
  }
}

inline void enterSleepAndAttachInterrupt(void)
{
  cli();
  interruptsOnPin0=0;
  f_wdt=0;
  /* Setup pin2 as an interrupt and attach handler. */
            // digitalPinToInterrupt(2) => 0
            // digitalPinToInterrupt(3) => 1
            // digitalPinToInterrupt(1 | 4) => -1
  attachInterrupt(digitalPinToInterrupt(interruptPin0), pin0InterruptRoutine, LOW); // can be LOW / CHANGE / HIGH / ...

  enterSleepNoInterrupt();
}

inline void enterSleepNoInterrupt(void)
{
  cli();
  f_wdt=0;
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
  pinMode(interruptPin0, INPUT);

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

  if (true) {
    setupWdt(true, WDT_PRSCL_8s);
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

void loop()
{
  cli();// disable interrupts (sei() = enable)
    // !!! delay() will not work when interrupt is disabled!
  if (f_wdt == 0) {
    // not watchdog interrupt!
    // so just put back to sleep
    enterSleepNoInterrupt();
  } else if (f_wdt == 1) {

    {
      // because delay will not work - we enable interrupt!
      //detachInterrupt(digitalPinToInterrupt(interruptPin0));
      //sei();
    }
    
    // wake-up from sleep
    digitalWrite(zoomerPin, HIGH);
      Serial.print("Ints: ");
      Serial.println(interruptsOnPin0, HEX);
      {
      Serial.flush();
      //delay(500);
      }
    digitalWrite(zoomerPin, LOW);
    wdt_reset();
    // put to sleep with interrupt
    enterSleepAndAttachInterrupt();
  } else {
    Serial.println("Program hanged!");
    delay(2000);
  }
}

