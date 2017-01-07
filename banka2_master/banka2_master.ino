#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <RF24.h>

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
#define BANKA_TRANSMISSION_SIZE 11
volatile byte transmission[BANKA_TRANSMISSION_SIZE];

SoftwareSerial softSerial(4, 3);
NullSerial nullSerial;

#define LOG Serial
#define DEBUG LOG // might be nullSerial to remove debug logs
GSMUtils gsmUtils(&softSerial, &LOG);
WDTHandler wdtHandler(&transmission[0], &LOG);
GSMInitializeProcessor gsmInitializeProcessor(gsmModuleResetPin, &gsmUtils, &LOG);
GSMSendSMSProcessor gsmSendSMSProcessor(&gsmUtils, &LOG, &wdtHandler);
GSMGetTimeProcessor gsmGetTimeProcessor(&gsmUtils, &LOG);

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
  setupRadio(&radio);

  bankaTransmissionTopRetryFailures_timer = bankaTransmissionTopRetryFailures_TIMER_MINUTES;
  for (int i = 0; i < sizeof(bankaTransmissionTopRetryFailures); i++)
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

  setupWdt(true, WDT_PRSCL_1s);

  LOG.println(F("Initialization complete."));
//  for (int i = 0; i < 5; i++) {
//    wdt_reset();
//    digitalWrite(zoomerPin, HIGH);
//    delay(500);
//    wdt_reset();
//    digitalWrite(zoomerPin, LOW);
//    delay(500);
//    delay(2000);
//    LOG.print('D');
//  }
//
//  LOG.println(); LOG.println("Starting business, now!");
  // Start the radio listening for data
  radio.startListening();
}


void processGsmLine_debug()
{
  byte gsm_rx_buf_size = gsmUtils.getRxBufSize();
  volatile char* gsmRxBuf = gsmUtils.getRxBufHeader();

  DEBUG.println();
  DEBUG.print(" GSM:");
  //DEBUG.print(gsm_rx_buf_size);
  if (gsm_rx_buf_size > 0) {
    DEBUG.print('|');
    for (byte i = 0; i < gsm_rx_buf_size; i++) {
      DEBUG.print(*(gsmRxBuf + i));
    }
    DEBUG.print('|');
    
    //DEBUG.print((byte)(*gsmRxBuf), HEX);
    //if (gsm_rx_buf_size > 1)
    //{
    //  DEBUG.print('-');
    //  DEBUG.print((byte)*(gsmRxBuf + gsm_rx_buf_size - 1), HEX);
    //}
    //DEBUG.print('|');
  }
  DEBUG.println();
  DEBUG.flush();
}

void processRadioTransmission_debug()
{
  DEBUG.println();
  DEBUG.print(" ID:"); DEBUG.print(transmission[0], HEX);// Banka(R) ID
  DEBUG.print(" T:");  DEBUG.print(transmission[1], HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  DEBUG.print(" C:");  DEBUG.print(transmission[2], HEX);// Communication No
  DEBUG.print(" F:");  DEBUG.print(transmission[3], HEX);// Failed attempts to deliver this communication
  DEBUG.print(" W:");  DEBUG.print(transmission[4], HEX);// WDT overruns
  DEBUG.print(" M:");  DEBUG.print(transmission[5], HEX);// Mag State
  DEBUG.print(" L:");  DEBUG.print(transmission[6], HEX);// Light State
  DEBUG.print(" A:");  DEBUG.print(transmission[7], HEX);// Port A interrupts
  DEBUG.print(" B:");  DEBUG.print(transmission[8], HEX);// Port B interrupts
  uint16_t bankaVccAdc = transmission[9]<<8|transmission[10];
  float bankaVcc = 1.1 / float (bankaVccAdc + 0.5) * 1024.0;
  DEBUG.print(" VCC:"); DEBUG.print(bankaVcc);// Battery Voltage
  DEBUG.println();
  DEBUG.flush();
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

  if (!wdtHandler.isBankaIdValid(bankaId)) {
    LOG.print(F("Unknown BankaR ID: ")); LOG.println(bankaId, HEX);
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
        DEBUG.println(); DEBUG.println(F("Sending SMS: ONLINE status!")); DEBUG.flush();
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
            //DEBUG.print(F("NOTIF: Can notify again: ")); DEBUG.print(canNotifyAgain);
            //DEBUG.print(F("; isOutOfReach: ")); DEBUG.print(isOutOfReach);
            //DEBUG.print(F("; isAlarm: ")); DEBUG.print(isAlarm);
            //DEBUG.println(); DEBUG.flush();
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
        LOG.println(F("Cannot start GSM initialization!")); LOG.flush();
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
        DEBUG.println("GSM: switching to ACTIVE STATE");
        gsmState = 21;//GSM_STATE_ACTIVE; // FIXME revert back!
      }
      else if (s == GSMInitializeProcessor::State::ERROR)
      {
        LOG.println(F("Error happened during GSM initialization!")); LOG.flush();
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
        LOG.println(F("ERROR Sending SMS!")); LOG.flush();
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
      for (int i = 0; i < sizeof(bankaTransmissionTopRetryFailures); i++)
        bankaTransmissionTopRetryFailures[i] = 0;
    }
    else if (gsmState == 21)
    {
      if (!gsmGetTimeProcessor.retrieveTimeFromNetwork())
      {
        LOG.println(F("Cannot start retrieval procedure!")); LOG.flush();
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
        LOG.println(F("Couldn't retrieve time from network!")); LOG.flush();
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
    DEBUG.println(F("GSM says: <ECHO>")); DEBUG.flush();
  }
  else if (gsmUtils.gsmIsLine(&GSM_OK[0], sizeof(GSM_OK))) {
    gsmLineReceived = GSM_LINE_OK;
    DEBUG.println(F("GSM says: OK")); DEBUG.flush();
  }
  else if (gsmUtils.gsmIsLine(&GSM_ERROR[0], sizeof(GSM_ERROR))) {
    gsmLineReceived = GSM_LINE_ERROR;
    DEBUG.println(F("GSM says: ERROR")); DEBUG.flush();
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
      DEBUG.print(F("Sending SMS: "));
      DEBUG.print(F("ID: ")); DEBUG.print(_smsContent.devId, HEX); DEBUG.print(F(", "));
      if (_smsContent.magLevel > 0) { DEBUG.print(F("MAG: ")); DEBUG.print(_smsContent.magLevel); DEBUG.print(F(", ")); }
      if (_smsContent.lightLevel > 0) { DEBUG.print(F("LIG: ")); DEBUG.print(_smsContent.lightLevel); DEBUG.print(F(", ")); }
      if (_smsContent.digSensors) { DEBUG.print(F("AorB, ")); }
      if (_smsContent.deviceResetFlag) { DEBUG.print(F("Reset/PowerUp, ")); }
      if (_smsContent.wdtOverrunFlag) { DEBUG.print(F("WatchDog!, ")); }
      if (_smsContent.outOfReach) { DEBUG.print(F("Missing!, ")); }
      if (_smsContent.wdtOverruns > 0) { DEBUG.print(F("OV: ")); DEBUG.print(_smsContent.wdtOverruns); }
      DEBUG.println();
      DEBUG.flush();
}

