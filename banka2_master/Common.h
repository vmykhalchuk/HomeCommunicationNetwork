#ifndef Utils_h
#define Utils_h

#include "Arduino.h"

typedef enum { WDT_PRSCL_16ms = 0, WDT_PRSCL_32ms, WDT_PRSCL_64ms, WDT_PRSCL_125ms, 
    WDT_PRSCL_250ms, WDT_PRSCL_500ms, WDT_PRSCL_1s, WDT_PRSCL_2s, WDT_PRSCL_4s, WDT_PRSCL_8s } edt_prescaler ;

void setupWdt(boolean enableInterrupt, uint8_t prescaler);

class NullSerial : public Stream
{
  public:
    virtual int peek();
    virtual size_t write(uint8_t byte);
    virtual int read();
    virtual int available();
    virtual void flush();
};

#endif
