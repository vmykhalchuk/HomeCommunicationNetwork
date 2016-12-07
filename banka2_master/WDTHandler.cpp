#include "Arduino.h"
#include "WDTHandler.h"

WDTHandler::WDTHandler()
{
  for (byte i = 0; i < sizeof(BANKA_IDS); i++)
  {
    banka_states[i].outOfReach = MAXIMUM_NO_TRANSMISSION_MINUTES;
    banka_states[i].sendSMSWhenZero = 0; // when event happens - we plan to send SMS now
    
    banka_states[i].magLevel = 0;
    banka_states[i].lightLevel = 0;
    banka_states[i].digSensors = false;
    banka_states[i].deviceResetFlag = false;
  }
}

void WDTHandler::wdtMinuteEvent()
{
  for (byte i = 0; i < sizeof(BANKA_IDS); i++)
  {
    if (banka_states[i].outOfReach > 0)
      banka_states[i].outOfReach--;
    if (banka_states[i].sendSMSWhenZero > 0)
      banka_states[i].sendSMSWhenZero--;
  }
}

void WDTHandler::radioTxReceivedForBanka(byte bankaId)
{
  int bankaIdNo = getBankaIdNoByBankaId(bankaId);
  if (bankaIdNo == -1) return;
  banka_states[bankaIdNo].outOfReach = MAXIMUM_NO_TRANSMISSION_MINUTES; // reset presence of BankaId counter
}

bool WDTHandler::isBankaOutOfReach(byte bankaId)
{
  int bankaIdNo = getBankaIdNoByBankaId(bankaId);
  if (bankaIdNo == -1) return true; // for fake ID will always return as out of reach
  return banka_states[bankaIdNo].outOfReach == 0; // if counter is 0 - then we haven't received TX for this banka for MAXIMUM_NO_TRANSMISSION_MINUTES
}

int WDTHandler::getBankaIdNoByBankaId(byte bankaId)
{
  int bankaIdNo = -1;
  for (byte i = 0; i < sizeof(BANKA_IDS); i++) {
    if (bankaId == BANKA_IDS[i]) {
      bankaIdNo = i;
      break;
    }
  }
  return bankaIdNo;
}

bool WDTHandler::isBankaIdValid(byte bankaId)
{
  return getBankaIdNoByBankaId(bankaId) != -1;
}


