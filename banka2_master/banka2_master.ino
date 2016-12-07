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
const byte addressMaster[6] = "MBank";
const byte addressSlave[6] = "1Bank";

WDTHandler wdtHandler;

SoftwareSerial softSerial(4, 5);
NullSerial nullSerial;

GSMUtils gsmUtils(&softSerial, &Serial);
#define LOG Serial

//-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-

ISR(WDT_vect) // called once a minute roughly
{
  wdtHandler.wdtMinuteEvent();
}

boolean setupRadio(void)
{
  if (!radio.begin())
  {
    return false;
  }
  radio.stopListening();// stop to avoid reset MCU issues

  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(118); // 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  radio.openWritingPipe(addressSlave); // writing to Slave (Banka)
  radio.openReadingPipe(1, addressMaster);

  return true;
}

volatile boolean isRadioUp = true;
void putRadioDown()
{
  if (isRadioUp) {
    radio.powerDown();
  }
  isRadioUp = false;
}
void putRadioUp()
{
  if (!isRadioUp) {
    radio.powerUp();
  }
  isRadioUp = true;
}

void setup()
{
  setupRadio();

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


volatile byte transmission[BANKA_TRANSMISSION_SIZE];
volatile bool readingRadioData = false;

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
      LOG.print((byte)(*gsmRxBuf + gsm_rx_buf_size - 1), HEX);
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
    LOG.println("OK OK OK!!!!");
  }
  if (gsmUtils.gsmIsLineStartsWith(&GSM_CMGS[0], sizeof(GSM_CMGS))) {
    LOG.println("OK CMGS OK!!!!");
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


  LOG.print(" ID:"); LOG.print(transmission[0], HEX);// Banka(R) ID
  LOG.print(" T:");  LOG.print(transmission[1], HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  LOG.print(" C:");  LOG.print(transmission[2], HEX);// Communication No
  LOG.print(" F:");  LOG.print(transmission[3], HEX);// Failed attempts to deliver this communication
  LOG.print(" W:");  LOG.print(transmission[4], HEX);// WDT overruns
  LOG.print(" M:");  LOG.print(transmission[5], HEX);// Mag State
  LOG.print(" L:");  LOG.print(transmission[6], HEX);// Light State
  LOG.print(" A:");  LOG.print(transmission[7], HEX);// Port A interrupts
  LOG.print(" B:");  LOG.print(transmission[8], HEX);// Port B interrupts
}

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
}



