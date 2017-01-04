#include "Arduino.h"
#include "WDTHandler.h"

WDTHandler::WDTHandler(volatile byte* radioTransmissionBuf, Stream* logComm)
{
  this->radioTransmissionBuf = radioTransmissionBuf;
  this->LOG = logComm;
  
  for (byte i = 0; i < sizeof(BANKA_IDS); i++)
  {
    resetBankaState(BANKA_IDS[i]);
  }
}

void WDTHandler::resetBankaState(byte bankaId)
{
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return;
  int i = bankaNo;
    banka_states[i].alarm = false;
    banka_states[i].outOfReach = MAXIMUM_NO_TRANSMISSION_MINUTES;
    banka_states[i].sendSMSWhenZero = 0; // when event happens - we plan to send SMS now
    
    banka_states[i].magLevel = 0;
    banka_states[i].lightLevel = 0;
    banka_states[i].digSensors = false;
    banka_states[i].deviceResetFlag = false;
    banka_states[i].wdtOverrunFlag = false;

    banka_states[i].wdtOverruns = 0;
}

void WDTHandler::wdtMinuteEvent()
{
  LOG->print('W');
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
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return;
  banka_states[bankaNo].outOfReach = MAXIMUM_NO_TRANSMISSION_MINUTES; // reset presence of BankaId counter

  bool alarm = false;

  volatile byte magLevel = *(radioTransmissionBuf+5);
  if ((magLevel > MAG_LEVEL_TRESHOLD) && (magLevel > banka_states[bankaNo].magLevel))
    { alarm = true; banka_states[bankaNo].magLevel = magLevel; }

  volatile byte lightLevel = *(radioTransmissionBuf+6);
  if ((lightLevel > LIGHT_LEVEL_TRESHOLD) && (lightLevel > banka_states[bankaNo].lightLevel))
    { alarm = true; banka_states[bankaNo].lightLevel = lightLevel; }

  volatile bool digSensors = (*(radioTransmissionBuf+7) > 0) || (*(radioTransmissionBuf+8) > 0);
  if (digSensors)
    { alarm = true; banka_states[bankaNo].digSensors = true; }

  volatile bool deviceResetFlag = (*(radioTransmissionBuf+1) == 1);
  if (deviceResetFlag)
    { alarm = true; banka_states[bankaNo].deviceResetFlag = true; }

  volatile bool wdtOverrunFlag = (*(radioTransmissionBuf+1) == 2);
  if (wdtOverrunFlag)
    { alarm = true; banka_states[bankaNo].wdtOverrunFlag = true; }

  banka_states[bankaNo].wdtOverruns = *(radioTransmissionBuf+4);

  byte vccHByte = *(radioTransmissionBuf+9);
  byte vccLByte = *(radioTransmissionBuf+10);
  uint16_t bankaVccAdc = vccHByte<<8|vccLByte;
  float bankaVcc = 0;
  if (bankaVccAdc > 5) {
    bankaVcc = 1.1 / float (bankaVccAdc + 0.5) * 1024.0;
  }
  banka_states[bankaNo].batteryVcc = bankaVcc;

  if (alarm) banka_states[bankaNo].alarm = true;
}

bool WDTHandler::isBankaOutOfReach(byte bankaId)
{
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return true; // for fake ID will always return as out of reach
  return banka_states[bankaNo].outOfReach == 0; // if counter is 0 - then we haven't received TX for this banka for MAXIMUM_NO_TRANSMISSION_MINUTES
}

bool WDTHandler::isBankaAlarm(byte bankaId)
{
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return true; // for fake ID will always return as alarmed!
  return banka_states[bankaNo].alarm;
}

bool WDTHandler::canNotifyAgainWithSms(byte bankaId)
{
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return true; // for fake ID will always return as true, to notify of wrong BankaID
  return banka_states[bankaNo].sendSMSWhenZero == 0;
}

void WDTHandler::setSmsNotifyPending(byte bankaId)
{
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return;
  banka_states[bankaNo].sendSMSWhenZero = SEND_NEXT_SMS_DELAY_MINUTES;
}

int WDTHandler::getBankaNoByBankaId(byte bankaId)
{
  int bankaNo = -1;
  for (byte i = 0; i < sizeof(BANKA_IDS); i++) {
    if (bankaId == BANKA_IDS[i]) {
      bankaNo = i;
      break;
    }
  }
  return bankaNo;
}

bool WDTHandler::isBankaIdValid(byte bankaId)
{
  return getBankaNoByBankaId(bankaId) != -1;
}

BankaState* WDTHandler::getBankaState(byte bankaId)
{
  int bankaNo = getBankaNoByBankaId(bankaId);
  if (bankaNo == -1) return NULL;
  return &banka_states[bankaNo];
}

