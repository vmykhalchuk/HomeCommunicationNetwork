#ifndef MiscHelper_h
#define MiscHelper_h

#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

class MiscHelper
{
  public:
    MiscHelper(Adafruit_HMC5883_Unified* mag, byte lightSensorPin, bool magSensorEnabled);

    byte getLightSensorState();
    
    // 1 - delta is [0-5)
    // 2 - delta is [5-10)
    // 3 - delta is [10-15)
    // 4 - delta is [15-20)
    // 5 - delta is [20-30)
    // 6 - delta is [30-40)
    // 7 - delta is [40-60)
    // 8 - delta is [60-100)
    // 9 - delta is [100-?)
    // 0 - not initialized yet
    // 0xF - disabled
    byte getMagSensorState();
  private:
    Adafruit_HMC5883_Unified* mag;
    byte lightSensorPin;
    bool magSensorEnabled;
    
    float _mss_data[15][3];
    int _mss_head = 0;
    boolean _mss_initialized = false;
};


#endif
