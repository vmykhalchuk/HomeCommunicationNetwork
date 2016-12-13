#include "Arduino.h"
#include "GSMAbstrProcessor.h"

/*  TIMER RELATED ACTIVITY */

// internal processing mechanism here
/*void GSMAbstractProcessor::_timerHandler()
{
  // timed out waiting for acknowledge from GSM module, failing
  _state = GSMSendSMSProcessorState::ERROR;
}*/

void GSMAbstractProcessor::_setTimer(int seconds)
{
  _timer = seconds;
  _timerActive = true;
}

void GSMAbstractProcessor::_deactivateTimer()
{
  _timerActive = false;
}

// called externally every second
void GSMAbstractProcessor::secondEventHandler()
{
  if (!_timerActive) return;
  if (_timer == 0)
  {
    _timerActive = false;
    _timerHandler();
  }
  else
  {
    _timer--;
  }
}

