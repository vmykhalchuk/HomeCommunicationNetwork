#include "Arduino.h"
#include "GSMInitializeProcessor.h"

GSMInitializeProcessor::GSMInitializeProcessor(GSMUtils* gsmUtils, Stream* logComm)
{
  this->gsmUtils = gsmUtils;
  this->LOG = logComm;
}

void GSMInitializeProcessor::_timerHandler()
{
  // timed out waiting for acknowledge from GSM module, failing
  _state = State::ERROR;
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
  
}

bool GSMInitializeProcessor::gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize)
{
  if (gsmLineReceived == GSM_LINE_NONE || gsmLineReceived == GSM_LINE_EMPTY)
  {
  }
  else if (gsmLineReceived == GSM_LINE_OK)
  {
    /*if (_state == State::S1)
    {
      _state = State::S2;
      _deactivateTimer();
      return true;
    }*/
  }
  return false;
}

