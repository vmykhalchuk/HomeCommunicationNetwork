#ifndef HomeCommNetworkCommon_h
#define HomeCommNetworkCommon_h

#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"

class HomeCommNetworkCommon {
  public:
	
	//Create up to 6 pipe addresses P0 - P5;  the "LL" is for LongLong type
	const uint64_t rAddress[6] = {0x7878787878LL, 0xB3B4B5B6F1LL, 0xB3B4B5B6CDLL, 0xB3B4B5B6A3LL, 0xB3B4B5B60FLL, 0xB3B4B5B605LL };
    //const byte addressMaster[6] = "MBank";
    //const byte addressSlave[6] = "SBank";
    //const byte addressRepeater1[6] = "1Bank";
    //const byte addressRepeater2[6] = "2Bank";
	
	bool setupRadio(RF24* radio);// constructor didn't work :( , so we must not to forget to call this method!
	void putRadioDown();
	void putRadioUp();
  private:
    RF24* radio;
	bool isRadioInitialized = false;
    bool isRadioUp = true;
};

#endif