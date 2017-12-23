#ifndef GSMInitializeProcessor_h
#define GSMInitializeProcessor_h

#include "Arduino.h"
#include <Stream.h>
#ifdef _PROJECT_ENABLE_DEBUG
  #define SERIAL_DEBUG Serial
#endif
#include <VMUtils_Misc.h>
#include "GSMAbstrProcessor.h"
#include "Common.h"
#include "GSMUtils.h"

class GSMInitializeProcessor : public GSMAbstractProcessor
{
  public:
    enum State {ZERO = 0, S1, S2, S3, S4, S5, WAITING_FOR_AT_ECHO, WAITING_FOR_OK_AFTER_AT_ECHO, RECEIVED_AT_AND_OK, WAITING_FOR_SMS_READY_REPORT, SUCCESS, ERROR};
    GSMInitializeProcessor(byte gsmModuleResetPin, GSMUtils* gsmUtils);

    bool startGSMInitialization();

    State processState(); // should be called as often as possible, takes minimum time to run (simulation of multithreading)
    
    /**
     * gsmLineReceived - type of line received (see Common.h # gsm_line_received)
     * lineStr and lineSize - line without eol terminators
     * return - true if line was received by this handler and processed
     */
    bool gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize); 
  private:
    byte gsmModuleResetPin;
    GSMUtils* gsmUtils;

    State _state = State::ZERO;

    void _timerHandler();
};

#endif
