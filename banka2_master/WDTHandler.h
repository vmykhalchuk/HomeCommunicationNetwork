#ifndef WDTHanlder_h
#define WDTHanlder_h

#include "Arduino.h"

const byte BANKA_IDS[] = { 0x11, 0x12, 0x21 }; // FIXME Move to INO file
#define SEND_NEXT_SMS_DELAY_MINUTES 250    // check how to define it in INO file
#define MAXIMUM_NO_TRANSMISSION_MINUTES 10 // check how to define it in INO file

struct BankaState
{
  byte outOfReach = MAXIMUM_NO_TRANSMISSION_MINUTES; // is updated by wdt
  byte sendSMSWhenZero = 0; // decremented by wdt, when sms was successfully sent - will be set to SEND_NEXT_SMS_DELAY_MINUTES
  
  byte magLevel = 0;
  byte lightLevel = 0;
  bool digSensors = false;
  bool deviceResetFlag = false;
};

class WDTHandler
{
  public:
    WDTHandler();
    void wdtMinuteEvent();
    int getBankaIdNoByBankaId(byte bankaId); // returns -1 if not found
    bool isBankaIdValid(byte bankaId);
    void radioTxReceivedForBanka(byte bankaId);
    bool isBankaOutOfReach(byte bankaId);
    //void dash();
  private:
    volatile BankaState banka_states[sizeof(BANKA_IDS)];
};

#endif
