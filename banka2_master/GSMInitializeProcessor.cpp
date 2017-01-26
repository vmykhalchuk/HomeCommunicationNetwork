#include "Arduino.h"
#include "GSMInitializeProcessor.h"

GSMInitializeProcessor::GSMInitializeProcessor(byte gsmModuleResetPin, GSMUtils* gsmUtils)
{
  this->gsmModuleResetPin = gsmModuleResetPin;
  this->gsmUtils = gsmUtils;
}

void GSMInitializeProcessor::_timerHandler()
{
  if (_state == State::S2)
  {
    _state = State::S3;
  }
  else if (_state == State::S4)
  {
    _state = State::S5;
  }
  else
  {
    // timed out waiting for acknowledge from GSM module, failing
    _println(F("GSM ERROR: response timedout!"));
    _state = State::ERROR;
  }
}

bool GSMInitializeProcessor::startGSMInitialization()
{
  if (_state == State::ZERO || _state == State::SUCCESS)
  {
    _state = State::S1;
    return true;
  }
  else
  {
    return false;
  }
}

GSMInitializeProcessor::State GSMInitializeProcessor::processState()
{
  if (_state == State::S1)
  {
    _debugln(F("GSMInitProc[S1]"));
    digitalWrite(gsmModuleResetPin, LOW);
    _setTimer(5); // 5 seconds
    _state = State::S2;
  }
  else if (_state == State::S2)
  {
    // wait for timer to switch to next S3
  }
  else if (_state == State::S3)
  {
    _debugln(F("GSMInitProc[S3]"));
    digitalWrite(gsmModuleResetPin, HIGH);
    _setTimer(6); // 6 seconds
    _state = State::S4;
    // FIXME instead of delay here, we should make few attempts to send "AT"
  }
  else if (_state == State::S4) {} // wait for timer to switch to next S5
  else if (_state == State::S5)
  {
    _debugln(F("GSMInitProc[S5]"));
    gsmUtils->resetGsmRxChannel();
    //    processGsmLine_reset();
    gsmUtils->gsm_sendCharBuf[0] = 'A'; gsmUtils->gsm_sendCharBuf[1] = 'T'; gsmUtils->gsm_sendCharBuf_size=2;
    gsmUtils->getGsmComm()->println("AT"); gsmUtils->getGsmComm()->flush();
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
    _state = State::WAITING_FOR_AT_ECHO;
  }
  //else if (_state == State::WAITING_FOR_AT_ECHO) {} // wait for "AT" line from GSM
  //else if (_state == State::WAITING_FOR_OK_AFTER_AT_ECHO) {} // wait for "OK" line from GSM
  else if (_state == State::RECEIVED_AT_AND_OK)
  {
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
    _state = WAITING_FOR_SMS_READY_REPORT;
  }
  //else if (_state == State::WAITING_FOR_SMS_READY_REPORT) {} // wait of "SMS Ready" line from GSM
  
  return _state;
}

bool GSMInitializeProcessor::gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize)
{
  if (gsmLineReceived == GSM_LINE_NONE || gsmLineReceived == GSM_LINE_EMPTY)
  {
  }
  else if (gsmLineReceived == GSM_LINE_SAME_AS_SENT)
  {
    if (_state == State::WAITING_FOR_AT_ECHO)
    {
      _debugln(F("GSMInitProc[AT received]"));
      _state = State::WAITING_FOR_OK_AFTER_AT_ECHO;
      return true;
    }
  }
  else if (gsmLineReceived == GSM_LINE_OK)
  {
    if (_state == State::WAITING_FOR_OK_AFTER_AT_ECHO)
    {
      _debugln(F("GSMInitProc[OK received]"));
      _state = State::RECEIVED_AT_AND_OK;
      return true;
    }
  }
  else if (gsmLineReceived == GSM_LINE_OTHER)
  {
    if (_state == State::WAITING_FOR_SMS_READY_REPORT)
    {
      if (gsmUtils->gsmIsLine(&GSM_CPIN_READY[0], sizeof(GSM_CPIN_READY)) || gsmUtils->gsmIsLine(&GSM_CALL_READY[0], sizeof(GSM_CALL_READY)))
      {
        _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS); // rewind timer to give more time to module to initialize
        return true;
      }
      else if (gsmUtils->gsmIsLine(&GSM_SMS_READY[0], sizeof(GSM_SMS_READY)))
      {
        _state = State::SUCCESS;
        _debugln(F("GSM MODULE INITIALIZED!"));
        _deactivateTimer();
        return true;
      }
      else
      {
        _debugln(F("ERROR: GSM strange line received"));
      }
    }
  }
  return false;
}

