/**
 * DEFINE BEFORE UPLOAD :: START
 */
#define SERIAL_OUTPUT Serial
#define SERIAL_DEBUG Serial
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
#include <VMMiscUtils.h>

#include <HomeCommNetworkCommon.h>

#include "Common.h"
#include "GSMUtils.h"
#include "WDTHandler.h"
#include "GSMInitializeProcessor.h"
#include "GSMSendSMSProcessor.h"
#include "GSMGetTimeProcessor.h"

const byte zoomerPin = 13;
const byte gsmModuleResetPin = 5;

/* Hardware configuration:
   Set up nRF24L01 radio on SPI bus (13, ???) plus pins 7 & 8 */
RF24 radio(7, 8);
#define BANKA_TRANSMISSION_SIZE 12
volatile byte transmission[BANKA_TRANSMISSION_SIZE];

SoftwareSerial softSerial(4, 3);

GSMUtils gsmUtils(&softSerial);
WDTHandler wdtHandler(&transmission[0]);
GSMInitializeProcessor gsmInitializeProcessor(gsmModuleResetPin, &gsmUtils);
GSMSendSMSProcessor gsmSendSMSProcessor(&gsmUtils, &wdtHandler);
GSMGetTimeProcessor gsmGetTimeProcessor(&gsmUtils);

//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

#define GSM_NO_RESPONSE_SECONDS 120
volatile byte gsmStartupState = GSM_STARTUP_STATE_ZERO;
volatile int gsmWdtTimer = 0;

volatile int notificationMechanismCounter = 0;

      // 0 - gsm is not ready yet, MCU just started
      // 1 - gsm module active, waiting for new sms to send; 2 - sent for processing
volatile byte gsmState = GSM_STATE_ZERO; 
SMSContent _smsContent;

volatile int gsmSendNotificationCounter = 0;

#define bankaTransmissionTopRetryFailures_TIMER_MINUTES 60*24 // send status every 24 hours roughly
volatile int bankaTransmissionTopRetryFailures_timer = 0;
volatile byte bankaTransmissionTopRetryFailures[sizeof(BANKA_IDS)];

//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

volatile int wdt_counter = 0;
ISR(WDT_vect) // called once a second roughly
{
  if (wdt_counter++ >= 60) {
    wdtHandler.wdtMinuteEvent();
    if (bankaTransmissionTopRetryFailures_timer>0) bankaTransmissionTopRetryFailures_timer--;
    wdt_counter = 0;
  }

  if (gsmWdtTimer>0) gsmWdtTimer--;

  gsmSendSMSProcessor.secondEventHandler();
  gsmInitializeProcessor.secondEventHandler();
  gsmGetTimeProcessor.secondEventHandler();
}

void setup()
{
  setupRadio(radio);
  radio.openReadingPipe(1, addressMaster);

  bankaTransmissionTopRetryFailures_timer = bankaTransmissionTopRetryFailures_TIMER_MINUTES;
  for (byte i = 0; i < sizeof(bankaTransmissionTopRetryFailures); i++)
  {
    bankaTransmissionTopRetryFailures[i] = 0;
  }

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
//  for (int i = 0; i < 5; i++) {
//    wdt_reset();
//    digitalWrite(zoomerPin, HIGH);
//    delay(500);
//    wdt_reset();
//    digitalWrite(zoomerPin, LOW);
//    delay(500);
//    delay(2000);
//    _print('D');
//  }
//
//  _println(); _println("Starting business, now!");
  // Start the radio listening for data
  radio.startListening();
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

void processRadioTransmission_debug()
{
  _debugln();
  _debug(" ID:"); _debugF(transmission[0], HEX);// Banka(R) ID
  _debug(" T:");  _debugF(transmission[1], HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  _debug(" C:");  _debugF(transmission[2], HEX);// Communication No
  _debug(" F:");  _debugF(transmission[3], HEX);// Failed attempts to deliver this communication
  _debug(" W:");  _debugF(transmission[4], HEX);// WDT overruns
  _debug(" M:");  _debugF(transmission[5], HEX);// Mag State
  _debug(" L:");  _debugF(transmission[6], HEX);// Light State
  _debug(" A:");  _debugF(transmission[7], HEX);// Port A interrupts
  _debug(" B:");  _debugF(transmission[8], HEX);// Port B interrupts
  uint16_t bankaVccAdc = transmission[9]<<8|transmission[10];
  float bankaVcc = 1.1 / float (bankaVccAdc + 0.5) * 1024.0;
  _debug(" VCC:"); _debug(bankaVcc);// Battery Voltage
  _debug(" RC:"); _debug(transmission[11]);
  _debugln();
}


void processRadioTransmission()
{
  byte bankaId = transmission[0];
  byte communicationNo = transmission[2];
  byte failedAttempts = transmission[3];
  if (bankaId == 0 && transmission[1] == 0 && communicationNo == 0)
  {
    return; // HOTFIX >>> this looks like radio is off and we receive very often 0 result
  }
  
  processRadioTransmission_debug();

  // FIXME transmission[11] - this is relaying counter - we should check this one if it is not 0 - then this message came from relay

  if (!wdtHandler.isBankaIdValid(bankaId)) {
    _print(F("Unknown BankaR ID: ")); _printlnF(bankaId, HEX);
    return;
  }
  
  wdtHandler.radioTxReceivedForBanka(bankaId);

  int bankaNo = wdtHandler.getBankaNoByBankaId(bankaId);
  if (bankaTransmissionTopRetryFailures[bankaNo] < failedAttempts)
  {
    bankaTransmissionTopRetryFailures[bankaNo] = failedAttempts;
  }
}


volatile bool readingRadioData = false;
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

  // radio life loop
  if (!readingRadioData && radio.available())
  {
    radio.readUnblockedStart((void*)(&transmission), sizeof(transmission));
    readingRadioData = true;
  }
  else if (readingRadioData && radio.readUnblockedFinished())
  {
    // read transmission from radio - its ready
    processRadioTransmission();
    readingRadioData = false;
  }

  // notification mechanism loop
  if (notificationMechanismCounter++ > 10 && gsmSendNotificationCounter > 1)
  {
    notificationMechanismCounter = 0;
    processNotification();
  }

  // GSM Send notification mechanism
  if (gsmSendNotificationCounter++ > 15 && notificationMechanismCounter > 1)
  {
    gsmSendNotificationCounter = 0;

    processGsmState();
  }
}

void processNotification()
{
    if (gsmState == GSM_STATE_ACTIVE)
    {
      // check if once a day notification mechanism has to be triggered (=zero if yes)
      if (bankaTransmissionTopRetryFailures_timer == 0)
      {
        _smsContent._type = 1;
        _smsContent.failuresArrSize = sizeof(bankaTransmissionTopRetryFailures);
        _smsContent.failuresArr = &(bankaTransmissionTopRetryFailures[0]);

        bankaTransmissionTopRetryFailures_timer = bankaTransmissionTopRetryFailures_TIMER_MINUTES; // rewind timer to trigger again in 24 hours
        _debugln(); _debugln(F("Sending SMS: ONLINE status!"));
        gsmState = GSM_STATE_REQUESTED_NOTIFICATION; // request send SMS state
      }
      else
      {
        // check if there is any alarm for Banka to be sent
        for (byte i = 0; i < sizeof(BANKA_IDS); i++)
        {
          byte bankaId = BANKA_IDS[i];
          bool canNotifyAgain = wdtHandler.canNotifyAgainWithSms(bankaId);
          bool isOutOfReach = wdtHandler.isBankaOutOfReach(bankaId);
          bool isAlarm = wdtHandler.isBankaAlarm(bankaId);
          if (canNotifyAgain && (isOutOfReach || isAlarm))
          {
            //_debug(F("NOTIF: Can notify again: ")); _debug(canNotifyAgain);
            //_debug(F("; isOutOfReach: ")); _debug(isOutOfReach);
            //_debug(F("; isAlarm: ")); _debug(isAlarm);
            //_debugln();
            _smsContent._type = 0;
            _smsContent.devId = bankaId;
            BankaState* bankaState = wdtHandler.getBankaState(bankaId);
            _smsContent.magLevel = bankaState->magLevel;
            _smsContent.lightLevel = bankaState->lightLevel;
            _smsContent.wdtOverruns = bankaState->wdtOverruns;
            _smsContent.outOfReach = isOutOfReach;
            _smsContent.digSensors = bankaState->digSensors;
            _smsContent.deviceResetFlag = bankaState->deviceResetFlag;
            _smsContent.wdtOverrunFlag = bankaState->wdtOverrunFlag;
          
            wdtHandler.resetBankaState(bankaId); // reset banka state - so new values can be loaded there
            logSendingSms();
            gsmState = GSM_STATE_REQUESTED_NOTIFICATION; break; // request send SMS state
          }
        }
      }
    }
}

void processGsmState()
{
    // GSM_STATE_ZERO; MCU started
    // GSM_STATE_INITIALIZING; initializing GSM module
    // GSM_STATE_ACTIVE; ready to process notification
    // GSM_STATE_REQUESTED_NOTIFICATION; processing notification

    if (gsmState == GSM_STATE_ZERO)
    {
      // start reset sequence
      if (!gsmInitializeProcessor.startGSMInitialization())
      {
        _println(F("Cannot start GSM initialization!"));
        delay(20000);
      }
      else
      {
        gsmState = GSM_STATE_INITIALIZING;
      }
    }
    else if (gsmState == GSM_STATE_INITIALIZING)
    {
      GSMInitializeProcessor::State s = gsmInitializeProcessor.processState();
      if (s == GSMInitializeProcessor::State::SUCCESS)
      {
        _debugln(F("GSM: switching to ACTIVE STATE"));
        gsmState = 21;//GSM_STATE_ACTIVE; // FIXME revert back!
      }
      else if (s == GSMInitializeProcessor::State::ERROR)
      {
        _println(F("Error happened during GSM initialization!"));
        delay(20000);
        gsmState = GSM_STATE_ZERO;
      }
    }
    else if (gsmState == GSM_STATE_ACTIVE)
    {
      // wait for Notification
    }
    else if (gsmState == GSM_STATE_REQUESTED_NOTIFICATION)
    {
      // send SMS now
      gsmState = 11;
    }
    else if (gsmState == 11)
    {
      if (!gsmSendSMSProcessor.sendSMS(0, &_smsContent))
      {
        _println(F("ERROR Sending SMS!"));
        gsmState = GSM_STATE_ZERO; // reset GSM //// FIXME This will also render Notification as obsollette and will never send it again!
      }
      else
      {
        gsmState = 12;
      }
    }
    else if (gsmState == 12)
    {
      // wait for gsmSendSMSProcessor to finish and check results
      GSMSendSMSProcessor::State s = gsmSendSMSProcessor.processState();
      if (s == GSMSendSMSProcessor::State::SUCCESS)
      {
        gsmState = 13;
      }
      else if (s == GSMSendSMSProcessor::State::ERROR)
      {
        gsmState = GSM_STATE_ACTIVE; // GSM_STATE_ZERO reset GSM //// FIXME This will also render Notification as obsollette and will never send it again!
      }
    }
    else if (gsmState == 13)
    {
      wdtHandler.setSmsNotifyPending(_smsContent.devId); // reset counter - so we do not send new sms again in short interval
      gsmState = GSM_STATE_ACTIVE; // ready for next notification to be processed

      //clean up for bankaTransmissionTopRetryFailures
      for (byte i = 0; i < sizeof(bankaTransmissionTopRetryFailures); i++)
        bankaTransmissionTopRetryFailures[i] = 0;
    }
    else if (gsmState == 21)
    {
      if (!gsmGetTimeProcessor.retrieveTimeFromNetwork())
      {
        _println(F("Cannot start retrieval procedure!"));
        gsmState = GSM_STATE_ZERO; // reset GSM //// FIXME This will also render Notification as obsollette and will never send it again!
      }
      else
      {
        gsmState = 22;
      }
    }
    else if (gsmState == 22)
    {
      GSMGetTimeProcessor::State s = gsmGetTimeProcessor.processState();
      if (s == GSMGetTimeProcessor::State::SUCCESS)
      {
        gsmState = GSM_STATE_ACTIVE;
      }
      else if (s == GSMGetTimeProcessor::State::ERROR)
      {
        _println(F("Couldn't retrieve time from network!"));
        gsmState = GSM_STATE_ACTIVE;
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
  else if (gsmUtils.gsmIsLine(&GSM_OK[0], sizeof(GSM_OK))) {
    gsmLineReceived = GSM_LINE_OK;
    _debugln(F("GSM says: OK"));
  }
  else if (gsmUtils.gsmIsLine(&GSM_ERROR[0], sizeof(GSM_ERROR))) {
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
    gsmGetTimeProcessor.gsmLineReceivedHandler(gsmLineReceived, gsmUtils.getRxBufHeader(), gsmUtils.getRxBufSize());
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

