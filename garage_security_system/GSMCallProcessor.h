#ifndef GSMCallProcessor_h
#define GSMCallProcessor_h

#include "Arduino.h"
#include <Stream.h>
#ifdef _PROJECT_ENABLE_DEBUG
  #define SERIAL_DEBUG Serial
#endif
#include <VMMiscUtils.h>
#include "GSMAbstrProcessor.h"
#include "Common.h"
#include "GSMUtils.h"

class GSMCallProcessor : public GSMAbstractProcessor
{
  public:
    enum State { ZERO = 0, ACTIVE, SendCOPS_2, SendCOPS_2__WAITING_OK, SendCTZU_1, SendCTZU_1__WAITING_OK, SendCOPS_0, SendCOPS_0__WAITING_OK, WAITING_CTZU, SUCCESS, ERROR };
    
    GSMCallProcessor(GSMUtils* gsmUtils, void (*receivedRingCallbackFunc)(), void (*receivedNumberCallbackFunc)(char* numberStr, int numberStrSize));

    bool retrieveTimeFromNetwork(); // start time retrieval procedure (false if cannot start due to some error!)
	bool callReceived();

	/**
	 * Returns State = INCOMING_CALL - when RING detected; or INCOMING_CALL_NUMBER - when caller number is known too
	 */
    State processState(); // should be called as often as possible, takes minimum time to run (simulation of multithreading)
    
    /**
     * gsmLineReceived - type of line received (see Common.h # gsm_line_received)
     * lineStr and lineSize - line without eol terminators
     * return - true if line was received by this handler and processed
     */
    bool gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize);
	
  private:
    GSMUtils* gsmUtils;
    void (*receivedRingCallbackFunc)();
    void (*receivedNumberCallbackFunc)(char* numberStr, int numberStrSize);
    State _state = State::ZERO;

    void _timerHandler();
};

#endif
