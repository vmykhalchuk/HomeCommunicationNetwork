#include "Arduino.h"
#include "MiscHelper.h"

MiscHelper::MiscHelper(Adafruit_HMC5883_Unified* mag, byte lightSensorPin, bool magSensorEnabled) {
  this->mag = mag;
  this->lightSensorPin = lightSensorPin;
  this->magSensorEnabled = magSensorEnabled;
}

byte MiscHelper::getLightSensorState()
{
  digitalWrite(this->lightSensorPin, HIGH); // enable pull-up before reading, otherwise no voltage dividor is working
  int v = 1023 - constrain(analogRead(this->lightSensorPin), 0, 1023);
  digitalWrite(this->lightSensorPin, LOW); // disable pull-up to save energy
  return map(v, 0, 1023, 0, 255);
}

byte MiscHelper::getMagSensorState()
{
  if (!this->magSensorEnabled)
    return 0xF;
    
  // Get a new sensor event
  sensors_event_t event; 
  this->mag->getEvent(&event);

  this->_mss_data[this->_mss_head][0] = event.magnetic.x;
  this->_mss_data[this->_mss_head][1] = event.magnetic.y;
  this->_mss_data[this->_mss_head][2] = event.magnetic.z;
  
  this->_mss_head++;
  
  if (!this->_mss_initialized) {
    if (this->_mss_head == 15) {
      this->_mss_initialized = true;
    }
  }
  this->_mss_head = this->_mss_head % 15;

  if (this->_mss_initialized) {
    // now we can start algorithm
    float sumXTotal = 0;
    float sumYTotal = 0;
    float sumZTotal = 0;
    float sumXLastThree = 0;
    float sumYLastThree = 0;
    float sumZLastThree = 0;
    int pos = this->_mss_head;
    for (int i = 0; i < 15; i++) {
      sumXTotal += this->_mss_data[pos][0];
      sumYTotal += this->_mss_data[pos][1];
      sumZTotal += this->_mss_data[pos][2];
      if (i >= (15-3)) {
        sumXLastThree += this->_mss_data[pos][0];
        sumYLastThree += this->_mss_data[pos][1];
        sumZLastThree += this->_mss_data[pos][2];
      }
      pos = (pos + 1) % 15;
    }
    sumXTotal /= 15;
    sumYTotal /= 15;
    sumZTotal /= 15;
    sumXLastThree /= 3;
    sumYLastThree /= 3;
    sumZLastThree /= 3;

    float deltaX = sumXTotal - sumXLastThree; if (deltaX < 0) deltaX = -deltaX;
    float deltaY = sumYTotal - sumYLastThree; if (deltaY < 0) deltaY = -deltaY;
    float deltaZ = sumZTotal - sumZLastThree; if (deltaZ < 0) deltaZ = -deltaZ;
    float deltaSum = deltaX+deltaY+deltaZ;
    if (deltaSum < 5) {
      return 1;
    } else if (deltaSum < 10) {
      return 2;
    } else if (deltaSum < 15) {
      return 3;
    } else if (deltaSum < 20) {
      return 4;
    } else if (deltaSum < 30) {
      return 5;
    } else if (deltaSum < 40) {
      return 6;
    } else if (deltaSum < 60) {
      return 7;
    } else if (deltaSum < 100) {
      return 8;
    } else {
      return 9;
    }
  }
  return 0x0;
}

