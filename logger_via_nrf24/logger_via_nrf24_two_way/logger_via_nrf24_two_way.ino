/**
 * DEFINE BEFORE UPLOAD :: START
 */
#define ROLE 1 // 0 - Master; 1 - Slave
#define LOGGER_CHANNEL 10
/**
 * DEFINE BEFORE UPLOAD :: END
 */

#include <SPI.h>
#include "RF24.h"

const byte addressNodeM[6] = "MLogg"; // Master
const byte addressNode2[6]  = "2Logg"; // Node 2 (Master's slave)

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

  radio.setRetries(3, 3);

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
  if (ROLE == 0)
  {
    radio.openWritingPipe(addressNode2);
    radio.openReadingPipe(1, addressNodeM);
    radio.startListening();
  }
  else
  {
    radio.openWritingPipe(addressNodeM);
    radio.openReadingPipe(1, addressNode2);
    radio.startListening();
  }
}

byte transmission[32];

void transmit()
{
  radio.stopListening();
  radio.flush_tx();
  bool succ = radio.write(&transmission, sizeof(transmission));
  radio.startListening();
}

void loop()
{
  if (Serial.available())
  {
    byte i = 1;
    while (Serial.available())
    {
      byte b = Serial.read();
      transmission[i] = b;
      i++;
      if (i == 32) break;
    }
    transmission[0] = i-1;
    transmit();
  }

  if (radio.available())
  {
    radio.read(&transmission, sizeof(transmission));
    for (byte i = 0; (i < transmission[0]) && (i < 31); i++)
    {
      Serial.write(transmission[i+1]);
    }
  }
}

