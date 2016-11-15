static const byte DEV_ID = 0x11; // register this dev Id
static const byte SEND_TRANSMISSION_TRESHOLD = 6; // every tick is every 8 seconds,
                                                  // then 8*6 = 48 seconds between transmissions

void setup(void)
{
  Serial.begin(9600);
}

struct RegsiterNewEventData
{
  byte transmissionId = 0;// just running number from 0 to 255 and back to 0
  byte lastFailed = 0;
  byte sendTransmissionCounter = 0; // send after given amount of register event calls
  byte sendTransmissionTreshold = SEND_TRANSMISSION_TRESHOLD;

  byte wdtOverruns = 0;
  byte magSensorData = 0;
  byte lightSensorData = 0;
  byte portAData = 0;
  byte portBData = 0;
} _rneD;

void registerNewEvent(byte wdtOverruns, 
                      byte magSensorData, byte lightSensorData,
                      byte portAData, byte portBData)
{
  // update current values with most recent ones
  if (wdtOverruns > _rneD.wdtOverruns)
          _rneD.wdtOverruns = wdtOverruns;
  if (magSensorData > _rneD.magSensorData) 
          _rneD.magSensorData = magSensorData;
  if (lightSensorData > _rneD.lightSensorData) 
          _rneD.lightSensorData = lightSensorData;
  if (portAData > _rneD.portAData) 
          _rneD.portAData = portAData;
  if (portBData > _rneD.portBData) 
          _rneD.portBData = portBData;

  if (++_rneD.sendTransmissionCounter >= _rneD.sendTransmissionTreshold)
  {
    // send transmission with data in _rneD
    boolean succeeded = _transmitData();
    if (succeeded)
    {
      _rneD.transmissionId++;
      _rneD.lastFailed = 0;
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = SEND_TRANSMISSION_TRESHOLD;
      _rneD.wdtOverruns = 0;
      _rneD.magSensorData = 0;
      _rneD.lightSensorData = 0;
      _rneD.portAData = 0;
      _rneD.portBData = 0;
    }
    else
    {
      _rneD.lastFailed++; if (_rneD.lastFailed > 100) _rneD.lastFailed = 100;
      _rneD.sendTransmissionCounter = 0;
      _rneD.sendTransmissionTreshold = _getTreshold(_rneD.lastFailed);
    }
  }
}

byte _getTreshold(byte lastFailed)
{
  byte v1 = 0, v2 = 1;
  for (int i = 0; i < lastFailed; i++)
  {
    byte s = v1;
    v1 = v2;
    v2 = s + v2;
  }
  return v1;
}

byte _transmitData_c = 0;
boolean _transmitData()
{
  Serial.print(" TID: "); Serial.print(_rneD.transmissionId);
  Serial.print(" LF: "); Serial.print(_rneD.lastFailed);
  Serial.print(" STC: "); Serial.print(_rneD.sendTransmissionCounter);
  Serial.print(" STT: "); Serial.print(_rneD.sendTransmissionTreshold);
  Serial.println();
  if (_transmitData_c>5) return true;
  _transmitData_c++;
  return false;
}

void loop(void)
{
  registerNewEvent(5, 7, 0, 1, 0);
  delay(1000);
}
