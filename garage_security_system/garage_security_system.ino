/**
 * DEFINE BEFORE UPLOAD :: START
 */
#define _PROJECT_ENABLE_DEBUG
/**
 * DEFINE BEFORE UPLOAD :: END
 */


#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <RF24.h>

// This is in VMUtils library!
#include <ADCUtils.h>
#include <WDTUtils.h>
//#define SERIAL_OUTPUT Serial
#ifdef _PROJECT_ENABLE_DEBUG
  #define SERIAL_DEBUG Serial
#endif
#include <VMMiscUtils.h>

#include "Common.h"
#include "GSMUtils.h"
#include "WDTHandler.h"
#include "GSMInitializeProcessor.h"
#include "GSMSendSMSProcessor.h"
#include "GSMCallProcessor.h"

const byte zoomerPin = 13;
const byte gsmModuleResetPin = 5;

SoftwareSerial softSerial(4, 3);

GSMUtils gsmUtils(&softSerial);
WDTHandler wdtHandler;
GSMInitializeProcessor gsmInitializeProcessor(gsmModuleResetPin, true, &gsmUtils);
GSMSendSMSProcessor gsmSendSMSProcessor(&gsmUtils);
GSMCallProcessor gsmCallProcessor(&gsmUtils, &callReceivedRingCallbackFunc, &callReceivedNumberCallbackFunc);

/*

1) When received call from known number - ARM system; Clear ALARM
2) When received two calls from same registered number within one minute - DISARM system

3) When in ARMED mode and any sensor triggered - go into ALARM_SET_OFF state:
	in this state perform following sequence
		1) Select First registered number
		2) Make call to that number
		3) If busy or cannot be reached - select next number and goto step 2
		4) If number hang up on the call or picked a phone - then drop call; wait 1 minute and goto ARMED mode

*/
//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

#define GSM_NO_RESPONSE_SECONDS 120
enum ProgState { ZERO = 0, INITIALIZING, UNARMED, ARM_PENDING, ARMED, ARMED_ALARM_SET_OFF, REQUESTED_NOTIFICATION, S11, S12, ERROR };

volatile ProgState progState = ProgState::ZERO;
SMSContent _smsContent;

//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

volatile int wdt_counter = 0;
ISR(WDT_vect) // called once a second roughly
{
  if (wdt_counter++ >= 60) {
    wdtHandler.wdtMinuteEvent();
    wdt_counter = 0;
  }

  gsmSendSMSProcessor.secondEventHandler();
  gsmInitializeProcessor.secondEventHandler();
  gsmCallProcessor.secondEventHandler();
}

void setup()
{
  Serial.begin(57600);
  while (!Serial);
  softSerial.begin(9600);

  pinMode(zoomerPin, OUTPUT);
  pinMode(gsmModuleResetPin, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(zoomerPin, HIGH);
    delay(1000);
    digitalWrite(zoomerPin, LOW);
    delay(1000);
  }

  WDTUtils::setupWdt(true, WDTUtils::PRSCL::_1s);

  _println(F("Initialization complete."));
}


void processGsmLine_debug()
{
  byte gsm_rx_buf_size = gsmUtils.getRxBufSize();
  volatile char* gsmRxBuf = gsmUtils.getRxBufHeader();

  _debugln();
  _debug(" GSM:");
  //_debug(gsm_rx_buf_size);
  if (gsm_rx_buf_size > 0) {
    _debug('|');
    for (byte i = 0; i < gsm_rx_buf_size; i++) {
      _debug(*(gsmRxBuf + i));
    }
    _debug('|');
    
    //_debugF((byte)(*gsmRxBuf), HEX);
    //if (gsm_rx_buf_size > 1)
    //{
    //  _debug('-');
    //  _debugF((byte)*(gsmRxBuf + gsm_rx_buf_size - 1), HEX);
    //}
    //_debug('|');
  }
  _debugln();
}


volatile int processProgStateCounter = 0;
void loop()
{
  // gsm life loop
  gsmUtils._serialEvent();
  if (gsmUtils.isRxBufLineReady())
  {
    // gsm line received - read it and process
    processGsmLine();
    gsmUtils.resetGsmRxBufAfterLineRead();
  }

  // Programm logic State machine
  if (processProgStateCounter++ > 15)
  {
    processProgStateCounter = 0;

    processProgState();
  }
}

void processProgState()
{
    // ZERO; MCU started
    // INITIALIZING; initializing GSM module
    // UNARMED; ready to process notification

    if (progState == ProgState::ZERO)
    {
      // start reset sequence
      if (!gsmInitializeProcessor.startGSMInitialization())
      {
        _println(F("Cannot start GSM initialization!"));
        delay(20000);
		// FIXME Reset CPU here
      }
      else
      {
        progState = ProgState::INITIALIZING;
      }
    }
    else if (progState == ProgState::INITIALIZING)
    {
      GSMInitializeProcessor::State s = gsmInitializeProcessor.processState();
      if (s == GSMInitializeProcessor::State::SUCCESS)
      {
        _debugln(F("GSM: switching to UNARMED STATE"));
        progState = ProgState::UNARMED;
      }
      else if (s == GSMInitializeProcessor::State::ERROR)
      {
        _println(F("Error happened during GSM initialization!"));
        delay(20000);
		// FIXME Reset CPU here
        progState = ProgState::ZERO;
      }
    }
    else if (progState == ProgState::UNARMED)
    {
      // wait for Calls
    }
    else if (progState == ProgState::REQUESTED_NOTIFICATION)
    {
      // send SMS now
      progState = ProgState::S11;
    }
    else if (progState == ProgState::S11)
    {
      if (!gsmSendSMSProcessor.sendSMS(0, &_smsContent))
      {
        _println(F("ERROR Sending SMS!"));
        progState = ProgState::ZERO; // reset GSM //// FIXME This will also render Notification as obsollette and will never send it again!
      }
      else
      {
        progState = ProgState::S12;
      }
    }
    else if (progState == ProgState::S12)
    {
      // wait for gsmSendSMSProcessor to finish and check results
      GSMSendSMSProcessor::State s = gsmSendSMSProcessor.processState();
      if (s == GSMSendSMSProcessor::State::SUCCESS)
      {
        progState = ProgState::UNARMED;
      }
      else if (s == GSMSendSMSProcessor::State::ERROR)
      {
        progState = ProgState::UNARMED; // ProgState::ZERO reset GSM //// FIXME This will also render Notification as obsollette and will never send it again!
      }
    }
}

void processGsmLine()
{
  volatile byte gsmLineReceived; // see gsm_line_received enum (Common.h)
  
  processGsmLine_debug();

  if (gsmUtils.gsmIsEmptyLine())
  {
    gsmLineReceived = GSM_LINE_EMPTY;
  }
  else if (gsmUtils.gsmIsLine(&(gsmUtils.gsm_sendCharBuf[0]), gsmUtils.gsm_sendCharBuf_size))
  {
    gsmLineReceived = GSM_LINE_SAME_AS_SENT; // Echo from GSM module
    _debugln(F("GSM says: <ECHO>"));
  }
  else if (gsmUtils.gsmIsLine(&GSM_OK[0], sizeof(GSM_OK)))
  {
    gsmLineReceived = GSM_LINE_OK;
    _debugln(F("GSM says: OK"));
  }
  else if (gsmUtils.gsmIsLine(&GSM_ERROR[0], sizeof(GSM_ERROR)))
  {
    gsmLineReceived = GSM_LINE_ERROR;
    _debugln(F("GSM says: ERROR"));
  }
  else
  {
    gsmLineReceived = GSM_LINE_OTHER;
  }

  if (gsmLineReceived != GSM_LINE_EMPTY)
  {
    gsmSendSMSProcessor.gsmLineReceivedHandler(gsmLineReceived, gsmUtils.getRxBufHeader(), gsmUtils.getRxBufSize());
    gsmInitializeProcessor.gsmLineReceivedHandler(gsmLineReceived, gsmUtils.getRxBufHeader(), gsmUtils.getRxBufSize());
    gsmCallProcessor.gsmLineReceivedHandler(gsmLineReceived, gsmUtils.getRxBufHeader(), gsmUtils.getRxBufSize());
  }

  // clean received line state after all handlers have finished
  if (gsmLineReceived == GSM_LINE_SAME_AS_SENT) gsmUtils.gsm_sendCharBuf_size = 0; // reset this, so we do not receive same line as echo!
}

void logSendingSms()
{
  _debug(F("Sending SMS: "));
  _debug(F("ID: ")); _debugF(_smsContent.devId, HEX); _debug(F(", "));
  if (_smsContent.magLevel > 0) { _debug(F("MAG: ")); _debug(_smsContent.magLevel); _debug(F(", ")); }
  if (_smsContent.lightLevel > 0) { _debug(F("LIG: ")); _debug(_smsContent.lightLevel); _debug(F(", ")); }
  if (_smsContent.digSensors) { _debug(F("AorB, ")); }
  if (_smsContent.deviceResetFlag) { _debug(F("Reset/PowerUp, ")); }
  if (_smsContent.wdtOverrunFlag) { _debug(F("WatchDog!, ")); }
  if (_smsContent.outOfReach) { _debug(F("Missing!, ")); }
  if (_smsContent.wdtOverruns > 0) { _debug(F("OV: ")); _debug(_smsContent.wdtOverruns); }
  _debugln();
}

void callReceivedRingCallbackFunc()
{
  _debug(F("RING!"));
  _debugln();
}

void callReceivedNumberCallbackFunc(char* numberStr, int numberStrSize)
{
  _debug(F("CLIP:")); _debugF(numberStrSize, DEC);
  _debugln();
}
