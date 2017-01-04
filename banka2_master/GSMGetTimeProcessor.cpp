#include "Arduino.h"
#include "GSMGetTimeProcessor.h"

GSMGetTimeProcessor::GSMGetTimeProcessor(GSMUtils* gsmUtils, Stream* logComm)
{
  this->gsmUtils = gsmUtils;
  this->LOG = logComm;
}

bool GSMGetTimeProcessor::retrieveTimeFromNetwork()
{
  if (_state != State::ZERO && _state != State::ERROR && _state != State::SUCCESS)
  {
    return false;
  }
  _state = State::SendCOPS_2;
  return true;
}

GSMGetTimeProcessor::State GSMGetTimeProcessor::processState()
{
  /* D - Device; M - GSM Module; (remember that M will also Echo back all our comands)
=-=-=-= PuTYY =-=-=-=
D>AT+COPS=2
M>OK
D>AT+CTZU=1
M>OK
D>AT+COPS=0
M>OK
M>+CTZU: "15/05/06,17:25:42",-12,0
// it doesn't work for me!
=-=-=-= PuTYY =-=-=-=

Also setting CLTS to 1:
D>AT+CLTS=1
M>OK
D>AT&W
M>OK
This will write it to EEPROM to always calibrate time with network
Indeed it doesn't work for me :(
   */
  if (_state == State::SendCOPS_2)
  {
    gsmUtils->getGsmComm()->println("AT+COPS=2");
    gsmUtils->getGsmComm()->flush();
    _state = State::SendCOPS_2__WAITING_OK;
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
  }
  else if (_state == State::SendCOPS_2__WAITING_OK)
  {
  }
  else if (_state == State::SendCTZU_1)
  {
    gsmUtils->getGsmComm()->println("AT+CLTS=1");//"AT+CTZU=1");
    gsmUtils->getGsmComm()->flush();
    _state = State::SendCTZU_1__WAITING_OK;
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
  }
  else if (_state == State::SendCTZU_1__WAITING_OK)
  {
  }
  else if (_state == State::SendCOPS_0)
  {
    gsmUtils->getGsmComm()->println("AT+COPS=0");
    gsmUtils->getGsmComm()->flush();
    _state = State::SendCOPS_0__WAITING_OK;
    _setTimer(GSM_NO_RESPONSE_TIMEOUT_SECONDS);
  }
  else if (_state == State::SendCOPS_0__WAITING_OK)
  {
  }
  else if (_state == State::WAITING_CTZU)
  {
  }
  return _state;
}

bool GSMGetTimeProcessor::gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize)
{
  if (gsmLineReceived == GSM_LINE_NONE || gsmLineReceived == GSM_LINE_EMPTY)
  {
  }
  else if (gsmLineReceived == GSM_LINE_OK)
  {
    if (_state == State::SendCOPS_2__WAITING_OK)
    {
      _state = State::SendCTZU_1;
      _deactivateTimer();
      return true;
    }
    else if (_state == State::SendCTZU_1__WAITING_OK)
    {
      _state = State::SendCOPS_0;
      _deactivateTimer();
      return true;
    }
    else if (_state == State::SendCOPS_0__WAITING_OK)
    {
      _state = State::WAITING_CTZU;
      //_deactivateTimer();
      return true;
    }
  }
  else if (gsmUtils->gsmIsLineStartsWith(&GSM_CTZU[0], sizeof(GSM_CTZU)))
  {
    //+CTZU: "15/05/06,17:25:42",-12,0
    //FIXME Read time here
    _state = State::SUCCESS;
    _deactivateTimer();
    return true;
  }
  return false;
}

void GSMGetTimeProcessor::_timerHandler()
{
  // timed out waiting for acknowledge from GSM module, failing
  LOG->println("GSM ERROR: response timedout!");
  _state = State::ERROR;
}

