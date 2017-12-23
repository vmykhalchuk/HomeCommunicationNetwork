
// First define where to send output and debug information to
#define SERIAL_OUTPUT Serial
#define SERIAL_DEBUG Serial
#include <VMUtils_Misc.h>

void setup()
{
  Serial.begin(9600);
  while (!Serial);

  _printF(15, HEX); // notice we do not have to use full syntax: Serial.print(...)
  _debugF(33, OCT); // this is example of printing to debug
  _debugln("This is debug statement!");

  _print(F("will be this #: "));
  _print(67);
  _println(", and thats it.");
  _printlnF(0x67, HEX);
}

void loop()
{
}
