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

  radio.setPALevel(RF24_PA_MIN); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
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
  radio.openWritingPipe(addressReceiver);
  Serial.println("Transmitter ready!");
}

long d = 0;
byte transmission[32];

void transmit()
{
  bool succ = radio.write(&transmission, sizeof(transmission));
  if (succ)
  {
    Serial.print("OK ");
  }
  else
  {
    Serial.print("ERR ");
  }
  Serial.print(transmission[0]);
  /*for (int i = 0; i < transmission[0]; i++)
  {
    Serial.print(' ');
    Serial.print(transmission[i+1], HEX);
  }*/
  Serial.println();
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

    d = 0;
  }
  else
  {
    d++;
    if (d >= 5500000)
    {
      transmission[0] = 1;
      transmission[1] = '.';
      transmit();
      d = 0;
    }
  }
}

