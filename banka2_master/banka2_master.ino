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

const byte zoomerPin = 13;
const byte gsmModuleResetPin = 5;

/* Hardware configuration:
   Set up nRF24L01 radio on SPI bus (13, ???) plus pins 7 & 8 */
RF24 radio(7, 8);
#define BANKA_TRANSMISSION_SIZE 9
volatile byte transmission[BANKA_TRANSMISSION_SIZE];
volatile bool readingRadioData = false;

SoftwareSerial softSerial(4, 3);
NullSerial nullSerial;

#define LOG Serial
#define DEBUG LOG // might be nullSerial to remove debug logs
WDTHandler wdtHandler(&transmission[0], &LOG);
GSMUtils gsmUtils(&softSerial, &LOG);

//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

#define GSM_NO_RESPONSE_SECONDS 120
volatile byte gsmStartupState = GSM_STARTUP_STATE_ZERO;
volatile int gsmWdtTimer = 0;

volatile int notificationMechanismCounter = 0;

      // 0 - gsm is not ready yet, MCU just started
      // 1 - gsm module active, waiting for new sms to send; 2 - sent for processing
volatile byte gsmState = GSM_STATE_ZERO; 
volatile byte devId, magLevel, lightLevel, wdtOverruns;
volatile bool outOfReach, digSensors, deviceResetFlag, wdtOverrunFlag;

volatile int gsmSendNotificationCounter = 0;

char gsm_sendCharBuf[250];
byte gsm_sendCharBuf_size = 0;
volatile byte gsmLineReceived = GSM_LINE_NONE; // see gsm_line_received enum (Common.h)
volatile bool gsmLineReceivedOverflow = false;


//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

volatile int wdt_counter = 0;
ISR(WDT_vect) // called once a minute roughly
{
  if (wdt_counter++ >= 10) {
    wdtHandler.wdtMinuteEvent();
    wdt_counter = 0;
  }

  if (gsmWdtTimer>0) gsmWdtTimer--;
}

void setup()
{
  setupRadio(&radio);

  Serial.begin(9600);
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

  LOG.println("Initialization complete.");
  for (int i = 0; i < 5; i++) {
    wdt_reset();
    digitalWrite(zoomerPin, HIGH);
    delay(500);
    wdt_reset();
    digitalWrite(zoomerPin, LOW);
    delay(500);
    delay(2000);
    LOG.print('D');
  }

  LOG.println(); LOG.println("Starting business, now!");
  // Start the radio listening for data
  radio.startListening();
}


void processGsmLine_debug()
{
  byte gsm_rx_buf_size = gsmUtils.getRxBufSize();
  volatile char* gsmRxBuf = gsmUtils.getRxBufHeader();

  DEBUG.println();
  DEBUG.print(" GSM:");
  DEBUG.print(gsm_rx_buf_size);
  if (gsm_rx_buf_size > 0) {
    DEBUG.print('|');
    for (byte i = 0; i < gsm_rx_buf_size; i++) {
      DEBUG.print(*(gsmRxBuf + i));
    }
    DEBUG.print('|');
    DEBUG.print((byte)(*gsmRxBuf), HEX);
    if (gsm_rx_buf_size > 1)
    {
      DEBUG.print('-');
      DEBUG.print((byte)*(gsmRxBuf + gsm_rx_buf_size - 1), HEX);
    }
  }
  DEBUG.println('|');
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
  DEBUG.println();
  DEBUG.flush();
}


void processRadioTransmission()
{
  byte bankaId = transmission[0];
  byte communicationNo = transmission[2];
  if (bankaId == 0 && communicationNo == 0)
  {
    return; // HOTFIX >>> this looks like radio is off and we receive very often 0 result
  }
  
  processRadioTransmission_debug();

  if (!wdtHandler.isBankaIdValid(bankaId)) {
    LOG.print("Unknown BankaR ID: "); LOG.println(bankaId, HEX);
    return;
  }
  wdtHandler.radioTxReceivedForBanka(bankaId);
}


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
  if (notificationMechanismCounter++ > 10)
  {
    notificationMechanismCounter = 0;

    if (gsmState == GSM_STATE_ACTIVE)
    {
      for (byte i = 0; i < sizeof(BANKA_IDS); i++)
      {
        byte bankaId = BANKA_IDS[i];
        bool isOutOfReach = wdtHandler.isBankaOutOfReach(bankaId);
        bool isAlarm = wdtHandler.isBankaAlarm(bankaId);
        if (wdtHandler.canNotifyAgainWithSms(bankaId) && (isOutOfReach || isAlarm))
        {
          devId = bankaId;
          BankaState* bankaState = wdtHandler.getBankaState(bankaId);
          magLevel = bankaState->magLevel;
          lightLevel = bankaState->lightLevel;
          wdtOverruns = bankaState->wdtOverruns;
          outOfReach = isOutOfReach;
          digSensors = bankaState->digSensors;
          deviceResetFlag = bankaState->deviceResetFlag;
          wdtOverrunFlag = bankaState->wdtOverrunFlag;
          
          wdtHandler.resetBankaState(bankaId); // reset banka state - so new values can be loaded there
          gsmState = GSM_STATE_REQUESTED_NOTIFICATION; break; // request send SMS state
        }
      }
    }
  }

  // GSM Send notification mechanism
  if (gsmSendNotificationCounter++ > 15)
  {
    gsmSendNotificationCounter = 0;

    processGsmStartupState();
    processGsmState();
  }
}

void processGsmState()
{
    if (gsmState == GSM_STATE_ACTIVE || gsmState == GSM_STATE_ZERO)
    {
      // do nothing
    }
    else if (gsmState == GSM_STATE_REQUESTED_NOTIFICATION)
    {
      // send SMS now
      logSendingSms();

      gsmState = 3; // successfully sent, proceed
    }
    else if (gsmState == 3)
    {
      wdtHandler.setSmsNotifyPending(devId); // reset counter - so we do not send new sms again in short interval
      gsmState = GSM_STATE_ACTIVE; // ready for next notification to be processed
    }
}

void processGsmStartupState()
{
    // Fx - GSM is on duty or in error state, nothing to be done by this processor
    //    FF - is active and on duty
    //    FE - error starting up
    // 0 - MCU started
    // 1 - reset command dispatched
    // 2 - AT was sent, waiting for response
    // 6 - SMS ready received, ready to work

    if ((gsmStartupState & 0xF0) > 0)
    {
      // do nothing here, we are active and on duty
    }
    else if (gsmStartupState == GSM_STARTUP_STATE_ZERO)
    {// 0. MCU started
      DEBUG.println("gsmStartupState[0]"); DEBUG.flush();
      digitalWrite(gsmModuleResetPin, LOW);
      gsmWdtTimer = 5 /*5sec*/; gsmStartupState = 1;
    }
    else if (gsmStartupState == 1)
    {// 1. reset pin dispatched
      if (gsmWdtTimer == 0)
      {
        DEBUG.println("gsmStartupState[1]"); DEBUG.flush();
        digitalWrite(gsmModuleResetPin, HIGH);
        gsmWdtTimer = 6 /*6sec*/; gsmStartupState = 2; // <<< HOTFIX instead of calibrating this timeout, we better make few attempts to send "AT"
      }
    }
    else if (gsmStartupState == 2)
    {// 2. reset delay finished
      if (gsmWdtTimer == 0)
      {
        DEBUG.println("gsmStartupState[2]"); DEBUG.flush();
        processGsmLine_reset();
        gsm_sendCharBuf[0] = 'A'; gsm_sendCharBuf[1] = 'T'; gsm_sendCharBuf_size=2;
        gsmUtils.getGsmComm()->println("AT"); gsmUtils.getGsmComm()->flush();
        gsmWdtTimer = GSM_NO_RESPONSE_SECONDS; gsmStartupState = 3;
      }
    }
    else
    {
      bool rewindTimer = false;
      if (gsmWdtTimer == 0) // timedout, need to reset GSM
        { gsmStartupState = GSM_STARTUP_STATE_ZERO; LOG.println("ERROR: GSM No response!"); }


      if (gsmStartupState == 3)
      {// 3. AT command sent, echo received
        if (gsmLineReceived == GSM_LINE_SAME_AS_SENT)
          { 
            gsm_sendCharBuf_size = 0;
            rewindTimer = true; gsmStartupState = 4;
          }
      }
      else if (gsmStartupState == 4)
      {// 4. OK response received
        if (gsmLineReceived == GSM_LINE_OK)
          {rewindTimer = true; gsmStartupState = 5; }
      }
      else if (gsmStartupState == 5)
      {//5. some init lines received from GSM module, waiting for "SMS Ready" line to say that module is ready
        if (gsmLineReceived == GSM_LINE_NONE)
        {
        }
        else if (gsmLineReceived == GSM_LINE_OTHER)
        {
          if (gsmUtils.gsmIsLine(&GSM_CPIN_READY[0], sizeof(GSM_CPIN_READY)) || gsmUtils.gsmIsLine(&GSM_CALL_READY[0], sizeof(GSM_CALL_READY)))
          {
            rewindTimer = true;
          }
          else if (gsmUtils.gsmIsLine(&GSM_SMS_READY[0], sizeof(GSM_SMS_READY)))
          { //SMS ready received, GSM module is ready to action!
            rewindTimer = true; gsmStartupState = 0xFF; gsmState = GSM_STATE_ACTIVE; // HERE We finally make GSM Ready to receive notifications!
            DEBUG.println("GSM MODULE ACTIVE!");
          }
          else
          {
            LOG.println("ERROR: GSM strange line received");
          }
        }
        else
        {
          DEBUG.print("gsmStartupState == 5 : Wrong gsmLineReceived:"); DEBUG.println(gsmLineReceived); DEBUG.flush();
        }

        if (rewindTimer) gsmWdtTimer = GSM_NO_RESPONSE_SECONDS;
      }
      else
      {
        DEBUG.print("Wrong gsmStartupState:"); DEBUG.println(gsmStartupState); DEBUG.flush();
        gsmStartupState = GSM_STARTUP_STATE_ZERO; // reseting GSM module
      }
    }
}

void processGsmLine_reset()
{
  gsmUtils.resetGsmRxChannel();
  gsmLineReceived = GSM_LINE_NONE;
  gsmLineReceivedOverflow = false;
}

void processGsmLine()
{
  processGsmLine_debug();

  // 0 - none, 1 - same line as was written, 2 - OK, 3 - something else
  if (gsmLineReceived != GSM_LINE_NONE)
  {
    gsmLineReceivedOverflow = true;
    LOG.println("GSM: Overflow!"); // do not flush, this is sensitive code LOG.flush();
  }

  if (gsmUtils.gsmIsEmptyLine())
  {
    //gsmLineReceived = GSM_LINE_EMPTY; << this is workaround, normally we do not expect any empty lines
  }
  else if (gsmUtils.gsmIsLine(&gsm_sendCharBuf[0], gsm_sendCharBuf_size))
  {
    gsmLineReceived = GSM_LINE_SAME_AS_SENT; // Echo from GSM module
    DEBUG.println("GSM says: <ECHO>"); DEBUG.flush();
  }
  else if (gsmUtils.gsmIsLine(&GSM_OK[0], sizeof(GSM_OK))) {
    gsmLineReceived = GSM_LINE_OK;
    DEBUG.println("GSM says: OK"); DEBUG.flush();
  }
  else
  {
    gsmLineReceived = GSM_LINE_OTHER;
  }
  /*if (gsmUtils.gsmIsLineStartsWith(&GSM_CMGS[0], sizeof(GSM_CMGS))) {
    DEBUG.println("GSM says: CMGS");
    DEBUG.println(); DEBUG.print("HURA: "); DEBUG.println(gsmUtils.gsmReadLineInteger(sizeof(GSM_CMGS))); DEBUG.flush();
  }*/
  processGsmState();
  processGsmStartupState();
  gsmLineReceived = GSM_LINE_NONE;
}

void logSendingSms()
{
      DEBUG.print("Sending SMS: ");
      DEBUG.print("ID: "); DEBUG.print(devId, HEX); DEBUG.print(", ");
      if (magLevel > 0) { DEBUG.print("MAG: "); DEBUG.print(magLevel); DEBUG.print(", "); }
      if (lightLevel > 0) { DEBUG.print("LIG: "); DEBUG.print(lightLevel); DEBUG.print(", "); }
      if (digSensors) { DEBUG.print("AorB, "); }
      if (deviceResetFlag) { DEBUG.print("Reset/PowerUp, "); }
      if (wdtOverrunFlag) { DEBUG.print("WatchDog!, "); }
      if (outOfReach) { DEBUG.print("Missing!, "); }
      if (wdtOverruns > 0) { DEBUG.print("OV: "); DEBUG.print(wdtOverruns); }
      DEBUG.println();
      DEBUG.flush();
}

