#include "Arduino.h"
#include "GSMCallProcessor.h"

GSMCallProcessor::GSMCallProcessor(GSMUtils* gsmUtils, void (*receivedRingCallbackFunc)(), void (*receivedNumberCallbackFunc)(char* numberStr, int numberStrSize))
{
  this->gsmUtils = gsmUtils;
  this->receivedRingCallbackFunc = receivedRingCallbackFunc;
  this->receivedNumberCallbackFunc = receivedNumberCallbackFunc;
}

bool GSMCallProcessor::retrieveTimeFromNetwork()
{
  #ifdef GSMCallProcessor_DISABLED
    return true;
  #endif
  if (_state != State::ZERO && _state != State::ERROR && _state != State::SUCCESS)
  {
    return false;
  }
  _state = State::SendCOPS_2;
  return true;
}

GSMCallProcessor::State GSMCallProcessor::processState()
{
    return State::SUCCESS;
}

bool GSMCallProcessor::gsmLineReceivedHandler(byte gsmLineReceived, __attribute__((unused)) char* lineStr, __attribute__((unused)) int lineSize)
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
  else if (gsmUtils->gsmIsLineStartsWith(&GSM_RING[0], sizeof(GSM_RING)))
  {
    this->receivedRingCallbackFunc();
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

void GSMCallProcessor::_timerHandler()
{
  // timed out waiting for acknowledge from GSM module, failing
  _println(F("GSM ERROR: response timedout!"));
  _state = State::ERROR;
}

