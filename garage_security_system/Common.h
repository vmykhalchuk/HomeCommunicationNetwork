#ifndef Utils_h
#define Utils_h

#include "Arduino.h"
#include <RF24.h>

typedef enum { GSM_LINE_NONE = 0, GSM_LINE_EMPTY, GSM_LINE_SAME_AS_SENT, GSM_LINE_OK, GSM_LINE_ERROR, GSM_LINE_OTHER } gsm_line_received ;

typedef enum { GSM_STATE_ZERO = 0, GSM_STATE_INITIALIZING, GSM_STATE_ACTIVE, GSM_STATE_REQUESTED_NOTIFICATION, GSM_STATE_1 } gsm_state ;
typedef enum { GSM_STARTUP_STATE_ZERO = 0, GSM_STARTUP_STATE_1 } gsm_startup_state ;

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
