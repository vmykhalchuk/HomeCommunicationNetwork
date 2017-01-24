#include <ADCUtils.h>

void setup() {
  Serial.begin(9600);
  while(!Serial);
  Serial.println(ADCUtils::readVcc());
}

void loop() {
}