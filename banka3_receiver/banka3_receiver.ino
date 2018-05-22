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
#include <RF24.h>
#include <printf.h>

#include <VMUtils_ADC.h>
//#define SERIAL_OUTPUT Serial
#ifdef _PROJECT_ENABLE_DEBUG
  #define SERIAL_DEBUG Serial
#endif
#include <VMUtils_Misc.h>

#include <HomeCommNetworkCommon.h>

#define _debug__dig(x) _debug(x)

const byte zoomerPin = 13;
const byte gsmModuleResetPin = 5;

/* Hardware configuration:
   Set up nRF24L01 radio on SPI bus (13, ???) plus pins 7 & 8 */
RF24 radio(7, 8);
HomeCommNetworkCommon hommCommNetwork;
#define BANKA_TRANSMISSION_SIZE 32
uint8_t transmission[BANKA_TRANSMISSION_SIZE];

void setup()
{
  Serial.begin(57600);
  while (!Serial);
  printf_begin();
  
  if (!hommCommNetwork.setupRadio(&radio, false))
  {
    _println("Radio initialization Failed!");
  }
  radio.openReadingPipe(1, hommCommNetwork.addressMaster);

  _println(F("Initialization complete."));
  radio.startListening();
  radio.printDetails();
}

void logResetSinceTime(uint16_t resetSinceMinutes)
{
  uint16_t days = resetSinceMinutes / (24*60);
  uint16_t hours = resetSinceMinutes / 60;
  uint16_t minutesInLastHour = resetSinceMinutes % 60;

  _debug(days); _debug("d"); _debug(hours % 24); _debug("h"); _debug(minutesInLastHour); _debug("m");
}

void processRadioTransmission_debug()
{
  _debugln();
  _debug(" ID:"); _debugF(transmission[0], HEX);// Banka(R) ID
  _debug(" V:");  _debugF(transmission[1]&0xF0 >> 4, HEX);// protocol version (1, 2, .. 0xF)
  _debug(" T:");  _debugF(transmission[1]&0x0F, HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  uint16_t bankaVccAdc = transmission[2]<<8|transmission[3];
  float bankaVcc = 1.1 / float (bankaVccAdc + 0.5) * 1024.0;
  _debug(" VCC:"); _debug(bankaVcc);// Battery Voltage
  uint16_t resetSinceMinutes = transmission[4]<<8|transmission[5];
  _debug(" Rst:"); logResetSinceTime(resetSinceMinutes); // Reset since time
  _debugln();
  _debug("   Historical: - 2m- 2m- 2m- 2m- 2m-10m-20m-40m-80m-160-320-640-12 -25 -51 -"); _debugln();
  _debug("                                                     m   m   m  80m 60m 20m "); _debugln();
  _debug("        Light: -"); for (int i = 0; i < 15; i++) debugPrintPos(&transmission[6], i); _debugln();
  _debug("       Magnet: -"); for (int i = 0; i < 15; i++) debugPrintPos(&transmission[6+8], i); _debugln();
  _debug("          A_B: -"); for (int i = 0; i < 15; i++) debugPrintPos(&transmission[6+16], i); _debugln();
  _debugln();
}

void __debugPrint2DecNumber(uint8_t z) {
  if (z >= 10) _debug__dig(z / 10) else _debug("0");
  _debug__dig(z % 10);
  _debug("-");
}
void debugPrintPos(uint8_t * pointer, int pos) {
  uint8_t z = *(pointer + pos/2);
  if (pos%2 == 0) {
    // read low bits
    z &= 0x0f;
  } else {
    // read high bits
    z = z >> 4;
  }

  _debug(" ");
  __debugPrint2DecNumber(z);
}

volatile bool readingRadioData = false;
uint16_t z = 0;
void loop()
{
  if (z++ > 60000) {
    //radio.printDetails();
    z = 0;
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
    processRadioTransmission_debug();
    readingRadioData = false;
  }
}

