#ifndef GSMAbstractProcessor_h
#define GSMAbstractProcessor_h

#include "Arduino.h"
#include <Stream.h>
#include "Common.h"
#include "GSMUtils.h"

class GSMAbstractProcessor
{
  public:
    void secondEventHandler(); // should be called every second
  private:
    int _timer = 0;
    bool _timerActive = false;//if active and reaches 0 - it will time-out GSM as unreachable!
    
  protected:
    byte const SEND_SMS_GSM_NO_RESPONSE_SECONDS = 120;
    void _setTimer(int seconds);
    void _deactivateTimer();
    
    virtual void _timerHandler();
};

#endif
