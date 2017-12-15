#ifndef HomeCommNetworkCommon_h
#define HomeCommNetworkCommon_h

//Create up to 6 pipe addresses P0 - P5;  the "LL" is for LongLong type
const uint64_t rAddress[] = {0x7878787878LL, 0xB3B4B5B6F1LL, 0xB3B4B5B6CDLL, 0xB3B4B5B6A3LL, 0xB3B4B5B60FLL, 0xB3B4B5B605LL };

//const byte addressMaster[6] = "MBank";
////const byte addressSlave[6] = "SBank";
//const byte addressRepeater1[6] = "1Bank";
//const byte addressRepeater2[6] = "2Bank";

//enum HCNDevice
//{
//	MASTER = 0,
//	REPEATER_1,
//	REPEATER_2,
//	NONE = 10
//}

boolean setupRadio(RF24& radio)
{
  if (!radio.begin())
  {
    return false;
  }
  radio.stopListening();// stop to avoid reset MCU issues

  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(125); // 0-125; 118 = 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  return true;
}

volatile boolean isRadioUp = true;
void putRadioDown(RF24& radio)
{
  if (isRadioUp) {
    radio.powerDown();
  }
  isRadioUp = false;
}
void putRadioUp(RF24& radio)
{
  if (!isRadioUp) {
    radio.powerUp();
  }
  isRadioUp = true;
}

#endif