#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#include "ADCUtils.h"

void setup()
{
  Serial.begin(57600);
  while(!Serial);

  // read battery status (twice)
  float v = ADCUtils::readVcc();
  Serial.print("VCC: ");
  Serial.println(v);
  v = ADCUtils::readVcc();
  Serial.print("VCC: ");
  Serial.println(v);
}

void loop()
{
}

