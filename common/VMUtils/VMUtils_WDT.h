#ifndef VMUtils_WDT_h
#define VMUtils_WDT_h

#include "Arduino.h"

class VMUtils_WDT
{
  public:
	enum PRSCL { _16ms = 0, _32ms, _64ms, _125ms, _250ms, _500ms, _1s, _2s, _4s, _8s };
	
    static void setupWdt(boolean enableInterrupt, PRSCL prescaler);
};

#endif