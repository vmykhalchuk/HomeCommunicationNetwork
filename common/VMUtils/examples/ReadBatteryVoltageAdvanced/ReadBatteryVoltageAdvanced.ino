#include <VMUtils_ADC.h>

void setup() {
  Serial.begin(9600);
  while(!Serial);

  // This is example on how to calculate VCC from uint16_t returned from ADC (to avoid heavy computation if not necessary when this value will be stored for later use)
  uint16_t vccAdc = VMUtils_ADC::readVccAsUint();
  float vcc = 1.1 / float (vccAdc + 0.5) * 1024.0;
  // NOTE: 1.1 - is a REF voltage in volts of a given device. You might need to adjust this value for you specific device
  Serial.println(vcc);
}

void loop() {
}
