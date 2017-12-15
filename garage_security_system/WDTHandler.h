#ifndef WDTHanlder_h
#define WDTHanlder_h

#include "Arduino.h"
#include <Stream.h>
#ifdef _PROJECT_ENABLE_DEBUG
  #define SERIAL_DEBUG Serial
#endif
#include <VMMiscUtils.h>

#define SEND_NEXT_SMS_DELAY_MINUTES 30


class WDTHandler
{
  public:
    WDTHandler();
    void wdtMinuteEvent(); // must be called once a minute
};

#endif
