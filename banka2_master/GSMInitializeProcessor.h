#ifndef GSMInitializeProcessor_h
#define GSMInitializeProcessor_h

#include "Arduino.h"
#include <Stream.h>
#include "GSMAbstrProcessor.h"
#include "Common.h"
#include "GSMUtils.h"

class GSMInitializeProcessor : public GSMAbstractProcessor
{
  public:
    enum State {ZERO = 0, S1, S2, SUCCESS, ERROR};
    GSMInitializeProcessor(GSMUtils* gsmUtils, Stream* logComm);

    bool startGSMInitialization();

    State processState(); // should be called as often as possible, takes minimum time to run (simulation of multithreading)
    
    /**
     * gsmLineReceived - type of line received (see Common.h # gsm_line_received)
     * lineStr and lineSize - line without eol terminators
     * return - true if line was received by this handler and processed
     */
    bool gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize); 
  private:
    GSMUtils* gsmUtils;
    Stream* LOG;

    State _state = State::ZERO;

    void _timerHandler();
};

#endif
