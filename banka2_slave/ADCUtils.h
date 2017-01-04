#ifndef ADCUtils_h
#define ADCUtils_h

#include "Arduino.h"

class ADCUtils
{
  private:
    static void readVccInt();
  public:
    static float readVcc();
    static void readVcc(uint8_t& lowByte, uint8_t& highByte);

};

#endif
