#ifndef Misc_h
#define Misc_h

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

#endif
