/*
 * Connect Radio module:
 * - 
 * 
 */

/**
 * DEFINE BEFORE UPLOAD :: START
 */
#define LOGGER_CHANNEL 2
/**
 * DEFINE BEFORE UPLOAD :: END
 */

#include <SPI.h>
#include "RF24.h"

const byte addressReceiver[6] = "RLogg"; // Receiver
const byte addressSender[6]  = "SLogg"; // Sender

boolean setupRadio(RF24& radio)
{
  if (!radio.begin())
  {
    return false;
  }
  radio.stopListening();// stop to avoid reset MCU issues

  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(LOGGER_CHANNEL); // 118 = 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  return true;
}

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);

void setup()
{
  Serial.begin(57600);
  while(!Serial);

  if (!setupRadio(radio)) {
    while(true) {
    }
  }
  radio.openReadingPipe(1, addressReceiver);
  radio.startListening();

  Serial.println("Receiver started!");
}

byte transmission[32];
void loop()
{
  if (radio.available())
  {
    radio.read(&transmission, sizeof(transmission));
    for (byte i = 0; (i < transmission[0]) && (i < 31); i++)
    {
      Serial.write(transmission[i+1]);
    }
  }
}

