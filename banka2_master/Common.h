#ifndef Utils_h
#define Utils_h

#include "Arduino.h"
#include <RF24.h>

class NullSerial : public Stream
{
  public:
    virtual inline int peek() {return 0;}
    virtual inline size_t write(uint8_t byte) {(void)byte;return 0;}
    virtual inline int read() {return 0;}
    virtual int available() {return 0;}
    virtual void flush() {};
};

typedef enum { WDT_PRSCL_16ms = 0, WDT_PRSCL_32ms, WDT_PRSCL_64ms, WDT_PRSCL_125ms, 
    WDT_PRSCL_250ms, WDT_PRSCL_500ms, WDT_PRSCL_1s, WDT_PRSCL_2s, WDT_PRSCL_4s, WDT_PRSCL_8s } edt_prescaler ;

typedef enum { GSM_LINE_NONE = 0, GSM_LINE_EMPTY, GSM_LINE_SAME_AS_SENT, GSM_LINE_OK, GSM_LINE_ERROR, GSM_LINE_OTHER } gsm_line_received ;

typedef enum { GSM_STATE_ZERO = 0, GSM_STATE_INITIALIZING, GSM_STATE_ACTIVE, GSM_STATE_REQUESTED_NOTIFICATION, GSM_STATE_1 } gsm_state ;
typedef enum { GSM_STARTUP_STATE_ZERO = 0, GSM_STARTUP_STATE_1 } gsm_startup_state ;

void setupWdt(boolean enableInterrupt, uint8_t prescaler);

const byte addressMaster[6] = "MBank";
//const byte addressSlave[6] = "SBank";
const byte addressRepeater1[6] = "1Bank";
const byte addressRepeater2[6] = "2Bank";

bool setupRadio(RF24* radio);

void putRadioDown();
void putRadioUp();

struct SMSContent
{
  volatile bool _type;

  // type = 0
  volatile byte devId, magLevel, lightLevel, wdtOverruns;
  volatile bool outOfReach, digSensors, deviceResetFlag, wdtOverrunFlag;

  // type = 1
  volatile byte* failuresArr;
  volatile byte failuresArrSize;
};

#endif
