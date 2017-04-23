#ifndef GSMSendSMSProcessor_h
#define GSMSendSMSProcessor_h

#include "Arduino.h"
#include <Stream.h>
#ifdef _PROJECT_ENABLE_DEBUG
  #define SERIAL_DEBUG Serial
#endif
#include <VMMiscUtils.h>
#include "GSMAbstrProcessor.h"
#include "Common.h"
#include "GSMUtils.h"
#include "WDTHandler.h"

class GSMSendSMSProcessor : public GSMAbstractProcessor
{
  public:
    enum State { ZERO = 0, S1, S2, S3, S4, WAIT_FOR_TEXT_OF_SMS_REQUEST, SEND_TEXT_OF_SMS, WAIT_FOR_OK_AFTER_SENDING_SMS, SUCCESS, ERROR };
    
    GSMSendSMSProcessor(GSMUtils* gsmUtils, WDTHandler* wdtHandler);

    /**
     * senderNo - 0 - Nataliya; 1 - Me
     */
    bool sendSMS(byte senderNo, SMSContent* _smsContent);

    State processState(); // should be called as often as possible, takes minimum time to run (simulation of multithreading)
    
    /**
     * gsmLineReceived - type of line received (see Common.h # gsm_line_received)
     * lineStr and lineSize - line without eol terminators
     * return - true if line was received by this handler and processed
     */
    bool gsmLineReceivedHandler(byte gsmLineReceived, char* lineStr, int lineSize); 
  private:
    GSMUtils* gsmUtils;
    WDTHandler* wdtHandler;
    State _state = State::ZERO;

    void _timerHandler();

    byte senderNo;
    SMSContent* _smsContent;
};

#endif
