#include <VMUtils_ADC.h>

void setup() {
  Serial.begin(9600);
  while(!Serial);
  Serial.println(VMUtils_ADC::readVcc());
}

void loop() {
}
