#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"
#include "HomeCommNetworkCommon.h"

bool HomeCommNetworkCommon::setupRadio(RF24* radio) {
  this->radio = radio;
  if (!this->radio->begin())
  {
    return false;
  }
  this->radio->stopListening();// stop to avoid reset MCU issues

  this->radio->setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  this->radio->setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  this->radio->setChannel(125); // 0-125; 118 = 2.518 Ghz - Above most Wifi Channels (default is 76)
  this->radio->setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  this->isRadioInitialized = true;
  return true;
}

bool HomeCommNetworkCommon::setupRadio(RF24* radio, bool autoAck) {
  bool res = this->setupRadio(radio);
  if (res == true) {
	this->radio->setAutoAck(autoAck);
  }
  return res;
}

void HomeCommNetworkCommon::putRadioDown() {
  if (!this->isRadioInitialized) return;
  
  if (this->isRadioUp) {
    this->radio->powerDown();
    this->isRadioUp = false;
  }
}

void HomeCommNetworkCommon::putRadioUp() {
  if (!this->isRadioInitialized) return;
  
  if (!this->isRadioUp) {
    this->radio->powerUp();
    this->isRadioUp = true;
  }
}
