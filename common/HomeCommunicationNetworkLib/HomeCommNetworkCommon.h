#ifndef HomeCommNetworkCommon_h
#define HomeCommNetworkCommon_h

#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"

class HomeCommNetworkCommon {
  public:
	
    const byte addressMaster[6] = "MBank";
    const byte addressSlave[6] = "SBank";
    const byte addressRepeater1[6] = "1Bank";
    const byte addressRepeater2[6] = "2Bank";
	
	bool setupRadio(RF24* radio);// constructor didn't work :(,so we must not to forget to call this method!
	bool setupRadio(RF24* radio, bool autoAck); // autoAck is enabled by default, here you can disable it if needed
	void putRadioDown();
	void putRadioUp();
  private:
    RF24* radio;
	bool isRadioInitialized = false;
    bool isRadioUp = true;
};

#endif
