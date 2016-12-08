//#define GSMSerial Serial
//#define LOGSerial SoftwareSerial(4,5)

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

/* Hardware configuration: Set up nRF24L01 radio on SPI bus (13, ???) plus pins 7 & 8 */
RF24 radio(7, 8);
#define BANKA_TRANSMISSION_SIZE 9

volatile byte transmission[BANKA_TRANSMISSION_SIZE];
volatile bool readingRadioData = false;

SoftwareSerial softSerial(4, 5);
NullSerial nullSerial;

#define LOG softSerial
WDTHandler wdtHandler(&transmission[0], &LOG);
GSMUtils gsmUtils(&Serial, &LOG);

//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

volatile int wdt_counter = 0;

ISR(WDT_vect) // called once a minute roughly
{
  if (wdt_counter++ > 60) {
    wdtHandler.wdtMinuteEvent();
    wdt_counter = 0;
  }
}

void setup()
{
  setupRadio(&radio);

  Serial.begin(9600);
  while (!Serial);
  softSerial.begin(9600);

  pinMode(zoomerPin, OUTPUT);
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

  LOG.println();
  LOG.print(" GSM:");
  LOG.print(gsm_rx_buf_size);
  if (gsm_rx_buf_size > 0) {
    LOG.print('|');
    for (byte i = 0; i < gsm_rx_buf_size; i++) {
      LOG.print(*(gsmRxBuf + i));
    }
    LOG.print('|');
    LOG.print((byte)(*gsmRxBuf), HEX);
    if (gsm_rx_buf_size > 1)
    {
      LOG.print('-');
      LOG.print((byte)*(gsmRxBuf + gsm_rx_buf_size - 1), HEX);
    }
  }
  LOG.println('|');
  LOG.flush();
}

void processRadioTransmission_debug()
{
  LOG.println();
  LOG.print(" ID:"); LOG.print(transmission[0], HEX);// Banka(R) ID
  LOG.print(" T:");  LOG.print(transmission[1], HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  LOG.print(" C:");  LOG.print(transmission[2], HEX);// Communication No
  LOG.print(" F:");  LOG.print(transmission[3], HEX);// Failed attempts to deliver this communication
  LOG.print(" W:");  LOG.print(transmission[4], HEX);// WDT overruns
  LOG.print(" M:");  LOG.print(transmission[5], HEX);// Mag State
  LOG.print(" L:");  LOG.print(transmission[6], HEX);// Light State
  LOG.print(" A:");  LOG.print(transmission[7], HEX);// Port A interrupts
  LOG.print(" B:");  LOG.print(transmission[8], HEX);// Port B interrupts
  LOG.println();
  LOG.flush();
}

void processGsmLine()
{
  if (gsmUtils.gsmIsLine(&GSM_OK[0], sizeof(GSM_OK))) {
    LOG.println("GSM says: OK");
  }
  if (gsmUtils.gsmIsLineStartsWith(&GSM_CMGS[0], sizeof(GSM_CMGS))) {
    LOG.println("GSM says: CMGS");
    LOG.println(); LOG.print("HURA: "); LOG.println(gsmUtils.gsmReadLineInteger(sizeof(GSM_CMGS)));
  }
  processGsmLine_debug();
}


void processRadioTransmission()
{
  byte bankaId = transmission[0];
  byte txType = transmission[1];
  if (!wdtHandler.isBankaIdValid(bankaId)) {
    LOG.print("Unknown BankaR ID: "); LOG.println(bankaId, HEX);
    return;
  }
  wdtHandler.radioTxReceivedForBanka(bankaId);

  processRadioTransmission_debug();
}

int notificationMechanismCounter = 0;
byte notificationState = 0; // 0 - waiting for new one; 1 - sent for processing, ....
byte devId, magLevel, lightLevel, wdtOverruns;
bool outOfReach, digSensors, deviceResetFlag, wdtOverrunFlag;

int gsmSendNotificationCounter = 0;

void loop()
{
  // gsm life loop
  gsmUtils._serialEvent();
  if (gsmUtils.isRxBufLineReady()) {
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
  if (notificationMechanismCounter++ > 40)
  {
    notificationMechanismCounter = 0;

    if (notificationState == 0) {
      for (byte i = 0; i < sizeof(BANKA_IDS); i++)
      {
        byte bankaId = BANKA_IDS[i];
        bool isOutOfReach = wdtHandler.isBankaOutOfReach(bankaId);
        bool isAlarm = wdtHandler.isBankaAlarm(bankaId);
        if (wdtHandler.canNotifyAgainWithSms(bankaId) && (isOutOfReach || isAlarm))
        {
          if (isAlarm) LOG.println("isAlarm");
          if (isOutOfReach) LOG.println("isOutOfReach");
          LOG.flush();
          
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
          
          notificationState = 1;
          break;
        }
      }
    }
  }

  // GSM Send notification mechanism
  if (gsmSendNotificationCounter++ > 50)
  {
    gsmSendNotificationCounter = 0;
    
    if (notificationState == 1) {
      // send it now
      wdtHandler.setSmsNotifyPending(devId);
      LOG.println("Sending SMS:");
      LOG.print("ID: "); LOG.print(devId, HEX);
      LOG.print(", M: "); LOG.print(magLevel);
      LOG.print(", L: "); LOG.print(lightLevel);
      LOG.print(", D: "); LOG.print(digSensors ? 'Y' : 'N');
      LOG.print(", R: "); LOG.print(deviceResetFlag ? 'Y' : 'N');
      LOG.print(", WO: "); LOG.print(wdtOverrunFlag ? 'Y' : 'N');
      LOG.print(", OR: "); LOG.print(outOfReach ? 'Y' : 'N');
      LOG.print(", OV: "); LOG.print(wdtOverruns);
      LOG.println();
      LOG.flush();

      notificationState = 0; // ready for next notification to be sent
    }
  }
}



