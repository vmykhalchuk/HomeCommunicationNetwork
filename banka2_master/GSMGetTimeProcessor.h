#ifndef GSMGetTimeProcessor_h
#define GSMGetTimeProcessor_h

#include "Arduino.h"
#include <Stream.h>
#include <VMMiscUtils.h>
#include "GSMAbstrProcessor.h"
#include "Common.h"
#include "GSMUtils.h"

class GSMGetTimeProcessor : public GSMAbstractProcessor
{
  public:
    enum State { ZERO = 0, SendCOPS_2, SendCOPS_2__WAITING_OK, SendCTZU_1, SendCTZU_1__WAITING_OK, SendCOPS_0, SendCOPS_0__WAITING_OK, WAITING_CTZU, SUCCESS, ERROR };
    
    GSMGetTimeProcessor(GSMUtils* gsmUtils);

    bool retrieveTimeFromNetwork(); // start time retrieval procedure (false if cannot start due to some error!)

    State processState(); // should be called as often as possible, takes minimum time to run (simulation of multithreading)
    
    /**
     * gsmLineReceived - type of line received (see Common.h # gsm_line_received)
     * lineStr and lineSize - line without eol terminators
     * return - true if line was received by this handler and processed
     */
    bool gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize); 
  private:
    GSMUtils* gsmUtils;
    State _state = State::ZERO;

    void _timerHandler();

    int _dateYYYY;
    int _dateMMDD;
    int _timeHHMM;
    int _timeSS;
};

#endif
