#include "Arduino.h"
#include "WDTHandler.h"

WDTHandler::WDTHandler()
{
}

void WDTHandler::wdtMinuteEvent()
{
  _print('W'); _print_flush();
}

