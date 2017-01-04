#include "Arduino.h"
#include "ADCUtils.h"

float ADCUtils::readVcc()
{
  ADCSRA =  bit (ADEN);   // turn ADC on
  ADCSRA |= bit (ADPS0) |  bit (ADPS1) | bit (ADPS2);  // Prescaler of 128
  ADMUX = bit (REFS0) | bit (MUX3) | bit (MUX2) | bit (MUX1);

  delay (10);  // let it stabilize

  bitSet (ADCSRA, ADSC);  // start a conversion  
  while (bit_is_set(ADCSRA, ADSC))
    { }
  
  return 1.1 / float (ADC + 0.5) * 1024.0;
}
