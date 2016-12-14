#ifndef WDTHanlder_h
#define WDTHanlder_h

#include "Arduino.h"
#include <Stream.h>

const byte BANKA_IDS[] = { 0x11 };
#define SEND_NEXT_SMS_DELAY_MINUTES 30
#define MAXIMUM_NO_TRANSMISSION_MINUTES 5 // minimum 4 minutes, maximum 5 minutes

#define MAG_LEVEL_TRESHOLD 3
#define LIGHT_LEVEL_TRESHOLD 2

struct BankaState
{
  byte alarm = false; // either some change happened requiring our attention (notification to owner)
  byte outOfReach = MAXIMUM_NO_TRANSMISSION_MINUTES; // is updated by wdt, when is 0 - then alarm should be raised that BankaR is out of reach!
  byte sendSMSWhenZero = 0; // decremented by wdt, when sms was successfully sent - will be set to SEND_NEXT_SMS_DELAY_MINUTES

  byte magLevel = 0;
  byte lightLevel = 0;
  bool digSensors = false;
  bool deviceResetFlag = false;
  bool wdtOverrunFlag = false;

  byte wdtOverruns = 0;
};

class WDTHandler
{
  public:
    WDTHandler(volatile byte* radioTransmissionBuf, Stream* logComm);
    void wdtMinuteEvent(); // must be called once a minute
    
    int getBankaNoByBankaId(byte bankaId); // returns -1 if not found
    bool isBankaIdValid(byte bankaId);
    
    void radioTxReceivedForBanka(byte bankaId); // will read radioTx from radioTransmissionBuf and update BankaR states with most recent data
    bool isBankaOutOfReach(byte bankaId); // if there were no successfull contact with BankaR for MAXIMUM_NO_TRANSMISSION_MINUTES, then it will return true
    bool isBankaAlarm(byte bankaId); // if alarm triggered on BankaR and was not yet processed

    bool canNotifyAgainWithSms(byte bankaId); // if returns false - then too short time passed since last SMS notification
    void setSmsNotifyPending(byte bankaId); // call this from SMS handler - till SMS is sent - to avoid another sms being sent within short 

    void resetBankaState(byte bankaId);// call it to reset, do it only when banka is being processed!
    BankaState* getBankaState(byte bankaId);
  private:
    Stream* LOG;
    volatile byte* radioTransmissionBuf;
    BankaState banka_states[sizeof(BANKA_IDS)];
};

#endif
