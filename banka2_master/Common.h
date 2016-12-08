#ifndef Utils_h
#define Utils_h

#include "Arduino.h"
#include <RF24.h>

class NullSerial : public Stream
{
  public:
    virtual int peek();
    virtual size_t write(uint8_t byte);
    virtual int read();
    virtual int available();
    virtual void flush();
};

typedef enum { WDT_PRSCL_16ms = 0, WDT_PRSCL_32ms, WDT_PRSCL_64ms, WDT_PRSCL_125ms, 
    WDT_PRSCL_250ms, WDT_PRSCL_500ms, WDT_PRSCL_1s, WDT_PRSCL_2s, WDT_PRSCL_4s, WDT_PRSCL_8s } edt_prescaler ;

void setupWdt(boolean enableInterrupt, uint8_t prescaler);

const byte addressMaster[6] = "MBank";
const byte addressSlave[6] = "1Bank";

bool setupRadio(RF24* radio);

void putRadioDown();
void putRadioUp();

#endif
