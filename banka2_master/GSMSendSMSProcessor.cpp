#include "Arduino.h"
#include "GSMSendSMSProcessor.h"

GSMSendSMSProcessor::GSMSendSMSProcessor(GSMUtils* gsmUtils, Stream* logComm)
{
  this->gsmUtils = gsmUtils;
  this->LOG = logComm;
}

bool GSMSendSMSProcessor::sendSMS(byte senderNo, SMSContent* _smsContent)
{
  if (_state != GSMSendSMSProcessorState::ZERO && _state != GSMSendSMSProcessorState::ERROR && _state != GSMSendSMSProcessorState::OK)
  {
    return false;
  }
  this->senderNo = senderNo;
  this->_smsContent = _smsContent;
  _state = GSMSendSMSProcessorState::S1;
  return true;
}

GSMSendSMSProcessorState GSMSendSMSProcessor::processState()
{
  if (_state == GSMSendSMSProcessorState::S1)
  {
    // send "+AT+CMGF=1" to modem
    gsmUtils->getGsmComm()->println("+AT+CMGF=1");
    gsmUtils->getGsmComm()->flush();
    _state = GSMSendSMSProcessorState::S2;
    _setTimer(SEND_SMS_GSM_NO_RESPONSE_SECONDS);
  }
  else if (_state == GSMSendSMSProcessorState::S2)
  {
    // waiting for response from GSM module
  }
  else if (_state == GSMSendSMSProcessorState::S3)
  {
    // received confirmation (OK) from GSM module
    gsmUtils->getGsmComm()->print("AT+CMGS=\"");
    gsmUtils->getGsmComm()->flush();
    _state = GSMSendSMSProcessorState::S4;
  }
  else if (_state == GSMSendSMSProcessorState::S4)
  { // write phone number
    if (senderNo == 0)
    {
      gsmUtils->getGsmComm()->print("+380975411391");
    }
    else if (senderNo == 1)
    {
      gsmUtils->getGsmComm()->print("+380975411368");
    }
    gsmUtils->getGsmComm()->println("\"");
    gsmUtils->getGsmComm()->flush();
    
    _setTimer(SEND_SMS_GSM_NO_RESPONSE_SECONDS);
    _state = GSMSendSMSProcessorState::WAIT_FOR_TEXT_OF_SMS_REQUEST;
  }
  else if (_state == GSMSendSMSProcessorState::WAIT_FOR_TEXT_OF_SMS_REQUEST)
  {
    if (gsmUtils->gsmIsSMSTypeTextRequest())
    {
      _state = GSMSendSMSProcessorState::SEND_TEXT_OF_SMS;
    }
  }
  else if (_state == GSMSendSMSProcessorState::SEND_TEXT_OF_SMS)
  { // write message + Ctrl+Z
    Stream* gsmComm = gsmUtils->getGsmComm();
    gsmComm->print("ID: "); gsmComm->print(_smsContent->devId, HEX); gsmComm->print(", ");
    if (_smsContent->magLevel > 0) { gsmComm->print("MAG: "); gsmComm->print(_smsContent->magLevel); gsmComm->print(", "); }
    if (_smsContent->lightLevel > 0) { gsmComm->print("LIG: "); gsmComm->print(_smsContent->lightLevel); gsmComm->print(", "); }
    if (_smsContent->digSensors) { gsmComm->print("AorB, "); }
    if (_smsContent->deviceResetFlag) { gsmComm->print("Reset/PowerUp, "); }
    if (_smsContent->wdtOverrunFlag) { gsmComm->print("WatchDog!, "); }
    if (_smsContent->outOfReach) { gsmComm->print("Missing!, "); }
    if (_smsContent->wdtOverruns > 0) { gsmComm->print("OV: "); gsmComm->print(_smsContent->wdtOverruns); }
    gsmComm->print((char)26);
    gsmComm->flush();
    
    _state = GSMSendSMSProcessorState::WAIT_FOR_OK_AFTER_SENDING_SMS;
    _setTimer(SEND_SMS_GSM_NO_RESPONSE_SECONDS);
  }
  else if (_state == GSMSendSMSProcessorState::WAIT_FOR_OK_AFTER_SENDING_SMS)
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
    if (_state == GSMSendSMSProcessorState::S2)
    {
      _state = GSMSendSMSProcessorState::S3;
      _deactivateTimer();
      return true;
    }
    else if (_state == GSMSendSMSProcessorState::WAIT_FOR_OK_AFTER_SENDING_SMS)
    {
      _state = GSMSendSMSProcessorState::OK;
      _deactivateTimer();
      return true;
    }
  }
  return false;
}

void GSMSendSMSProcessor::_timerHandler()
{
  // timed out waiting for acknowledge from GSM module, failing
  _state = GSMSendSMSProcessorState::ERROR;
}

