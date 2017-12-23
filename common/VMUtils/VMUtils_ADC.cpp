#include "Arduino.h"
#include "VMUtils_ADC.h"

void VMUtils_ADC::_readVcc()
{
  ADCSRA =  bit (ADEN);   // turn ADC on
  ADCSRA |= bit (ADPS0) |  bit (ADPS1) | bit (ADPS2);  // Prescaler of 128
  ADMUX = bit (REFS0) | bit (MUX3) | bit (MUX2) | bit (MUX1);

  delay (10);  // let it stabilize

  bitSet (ADCSRA, ADSC);  // start a conversion  
  while (bit_is_set(ADCSRA, ADSC))
    { }

  // value is now available in ADC
}

float VMUtils_ADC::readVcc()
{
  _readVcc();
  return 1.1 / float (ADC + 0.5) * 1024.0;

  // 0x12 shows 4.41 for 4.24(compact multimeter) or 4.21(big multimeter)
  // 0x12 shows 4.37 for 4.20(compact multimeter) or 4.175(big multimeter)

  // 0x11 shows 4.39 for 4.31(compact multimeter) or 4.28(big multimeter)
  // 0x11 shows 4.37 for 4.??(compact multimeter) or 4.27(big multimeter)
}

uint16_t VMUtils_ADC::readVccAsUint()
{
  _readVcc();
  return ADC;
}

/*void VMUtils_ADC::readVcc(uint8_t& lowByte, uint8_t& highByte)
{
  readVccInt();
  uint16_t _adc = ADC;
  highByte = _adc>>8;
  lowByte = _adc&0x00FF;
}*/

