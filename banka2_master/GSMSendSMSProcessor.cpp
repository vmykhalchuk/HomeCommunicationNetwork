#include "Arduino.h"
#include "GSMSendSMSProcessor.h"

GSMSendSMSProcessor::GSMSendSMSProcessor(GSMUtils* gsmUtils, WDTHandler* wdtHandler)
{
  this->gsmUtils = gsmUtils;
  this->wdtHandler = wdtHandler;
}

bool GSMSendSMSProcessor::sendSMS(byte senderNo, SMSContent* _smsContent)
{
  if (_state != State::ZERO && _state != State::ERROR && _state != State::SUCCESS)
  {
    return false;
  }
  if (_smsContent->_type != 0 && _smsContent->_type != 1)
  {
    return false;
  }
  this->senderNo = senderNo;
  this->_smsContent = _smsContent;
  _state = State::S1;
  return true;
}

GSMSendSMSProcessor::State GSMSendSMSProcessor::processState()
{
  if (_state == State::S1)
  {
    // send "+AT+CMGF=1" to modem
    gsmUtils->getGsmComm()->println("+AT+CMGF=1");
    gsmUtils->getGsmComm()->flush();
    _state = State::S2;
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
  }
  else if (_state == State::S2)
  {
    // waiting for response from GSM module
  }
  else if (_state == State::S3)
  {
    // received confirmation (OK) from GSM module
    gsmUtils->getGsmComm()->print("AT+CMGS=\"");
    gsmUtils->getGsmComm()->flush();
    _state = State::S4;
  }
  else if (_state == State::S4)
  { // write phone number
    if (senderNo == 0)
    {
      gsmUtils->getGsmComm()->print("+380975411368");
    }
    else if (senderNo == 1)
    {
      gsmUtils->getGsmComm()->print("+380975411391");
    }
    gsmUtils->getGsmComm()->println("\"");
    gsmUtils->getGsmComm()->flush();
    
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
    _state = State::WAIT_FOR_TEXT_OF_SMS_REQUEST;
  }
  else if (_state == State::WAIT_FOR_TEXT_OF_SMS_REQUEST)
  {
    if (gsmUtils->gsmIsSMSTypeTextRequest())
    {
      _state = State::SEND_TEXT_OF_SMS;
    }
  }
  else if (_state == State::SEND_TEXT_OF_SMS)
  { // write message + Ctrl+Z
    Stream* gsmComm = gsmUtils->getGsmComm();
    if (_smsContent->_type == 0)
    {
      gsmComm->print("ID: "); gsmComm->print(_smsContent->devId, HEX); gsmComm->print(", ");
      if (_smsContent->magLevel > 0) { gsmComm->print("MAG: "); gsmComm->print(_smsContent->magLevel); gsmComm->print(", "); }
      if (_smsContent->lightLevel > 0) { gsmComm->print("LIG: "); gsmComm->print(_smsContent->lightLevel); gsmComm->print(", "); }
      if (_smsContent->digSensors) { gsmComm->print("AorB, "); }
      if (_smsContent->deviceResetFlag) { gsmComm->print("Reset/PowerUp, "); }
      if (_smsContent->wdtOverrunFlag) { gsmComm->print("WatchDog!, "); }
      if (_smsContent->outOfReach) { gsmComm->print("Missing!, "); }
      if (_smsContent->wdtOverruns > 0) { gsmComm->print("OV: "); gsmComm->print(_smsContent->wdtOverruns); }
    }
    else if (_smsContent->_type == 1)
    {
      gsmComm->println(F("ONLINE!"));
      /*for (int i = 0; i < _smsContent->failuresArrSize; i++)
      {
        gsmComm->print(" 0x"); gsmComm->print(*(_smsContent->failuresArr+i), HEX);
      }*/
      for (int i = 0; i < sizeof(BANKA_IDS); i++)
      {
        uint8_t bankaId = BANKA_IDS[i];
        BankaState* bankaState = wdtHandler->getBankaState(bankaId);
        gsmComm->print("ID: 0x"); gsmComm->print(bankaId, HEX);
        gsmComm->print(" f:0x"); gsmComm->print(*(_smsContent->failuresArr+i), HEX);
        gsmComm->print(" v:"); gsmComm->print(bankaState->batteryVcc);
        gsmComm->println();
      }
    }
    gsmComm->print((char)26);
    gsmComm->flush();
    
    _state = State::WAIT_FOR_OK_AFTER_SENDING_SMS;
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
  }
  else if (_state == State::WAIT_FOR_OK_AFTER_SENDING_SMS)
  {
    // waiting for response from GSM module
  }

  return _state;
}

bool GSMSendSMSProcessor::gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize)
{
  if (gsmLineReceived == GSM_LINE_NONE || gsmLineReceived == GSM_LINE_EMPTY)
  {
  }
  else if (gsmLineReceived == GSM_LINE_OK)
  {
    if (_state == State::S2)
    {
      _state = State::S3;
      _deactivateTimer();
      return true;
    }
    else if (_state == State::WAIT_FOR_OK_AFTER_SENDING_SMS)
    {
      _state = State::SUCCESS;
      _deactivateTimer();
      return true;
    }
  }
  else if (gsmLineReceived == GSM_LINE_OK)
  {
    if (_state == State::WAIT_FOR_OK_AFTER_SENDING_SMS)
    {
      _state = State::ERROR;
      _deactivateTimer();
      return true;
    }
  }
  return false;
}

void GSMSendSMSProcessor::_timerHandler()
{
  // timed out waiting for acknowledge from GSM module, failing
  _println(F("GSM ERROR: response timedout!"));
  _state = State::ERROR;
}

