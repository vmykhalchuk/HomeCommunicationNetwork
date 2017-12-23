
// First define where to send output and debug information to
#define SERIAL_OUTPUT Serial
//#define SERIAL_DEBUG Serial // If no SERIAL_DEBUG defined - then debug statements will not be included in final code at all!
#include <VMUtils_Misc.h>

void setup()
{
  Serial.begin(9600);
  while (!Serial);

  _printF(15, HEX); // notice we do not have to use full syntax: Serial.print(...)
  _debugF(33, OCT); // this is example of printing to debug
  _debugln("This is debug statement!");
}

void loop()
{
}
