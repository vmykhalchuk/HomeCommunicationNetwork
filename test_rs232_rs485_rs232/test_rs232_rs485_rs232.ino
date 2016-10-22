// If we have to organize secure RS232 communication via long wires
// we will use following schematic
// 
// [DEVICE_1].RS232 <=> RS232.[COM_DEV_1].RS485 <=> [long wires] <=> RS485.[COM_DEV_2].RS232 <=> RS232.[DEVICE_2]
//
// [COM_DEV_1] and [COM_DEV_2] are Arduino based devices with this firmware
//  each having:
//    port0, port1 = for RS232 communication (HardwareSerial)
//    port8(RX), port9(TX), port7(READ/WRITE) = for RS485 communication (SoftwareSerial)
//    port13 = for RS485 line status

#include <SoftwareSerial.h>

SoftwareSerial mySerial(8, 9); // RX, TX

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  while (!Serial) {
  }
  mySerial.begin(57600);

}

//            ____  10  _________
// END1           \____/         
//
//            _____   15   ______
// END2            \______/      



// First decide who is driving communication
// S0) wait for 100-300ms and check if there is any change happening on the COM1 (RS485)
//      A) nothing happens, line was constantly high
//         (no one is here, wait for it) => goto S1
//      B) there were some changes
//         (perhaps other end is waiting, we should try to respond) => goto S3
//      C) error, line was constantly low
//          ERR => show error, wait 1sec and then goto S0

// S1) pull line down for 10msec, and check line after that
//      A) line is high (no second end yet) => goto S1A
//      B) line is low (second end responded) => goto S2

// S1A) monitor line for 10msec
//      A) line was constantly high
//          (no second end yet, lets ty again) => goto S1
//      B) line was low (even once)
//          ERR => show error, wait 1sec and then goto S0

// S2) [we have other responded as slave] monitor line for 7msec
//      A) no change, line was and is low
//          ERR => show error, wait 1sec and then goto S0
//      B) changed many times
//          ERR => show error, wait 1sec and then goto S0
//      C) went up and is up till now
//          (contact established, have to verify it) => goto S2A

// S2A) monitor line for 200msec
//      A) nothing changed (everything OK) => goto S2B
//      B) line state changed
//          ERR => show error, wait 1sec and then goto S0

// S2B) init COM1 with 9600bod,def and send (0x5A, 0xA5), then wait for a reply
//      A) received 0x2E and 0xD1
//          (success, we are Master now) => goto S2C
//      B) received wrong bytes or nothing at all after given time
//          ERR => show error, wait 1sec and then goto S0

// S2C) init COM1 with required speed (COM0 speed * 4), send (0x5B, 0xA4) and wait for reply
//      A) received 0x2E and 0xD1
//          (total success, now we are master of disaster) => goto S4
//      B) received wrong bytes or nothing at all after given time
//          ERR => show error, wait 1sec and then goto S0

// S3) monitor COM1 before it gets UP again
//      A) it didn't go UP for 12msec
//          ERR => show error, wait 1sec and then goto S0
//      B) it went UP => goto S3A

// S3A) monitor COM1 to get DOWN
//      A) it didn't go DOWN for 12msec
//          ERR => show error, wait 1sec and then goto S0
//      B) it went DOWN in less then 8msec
//          ERR => show error, wait 1sec and then goto S0
//      C) it went DOWN within given timeframe
//          OK => goto S3B

// S3B) monitor COM1 to get UP
//      A) it didn't go UP for 12msec
//          ERR => show error, wait 1sec and then goto S0
//      B) it went UP in less then 8msec
//          ERR => show error, wait 1sec and then goto S0
//      C) it went UP within given timeframe
//          OK => goto S3C

// S3C) monitor COM1 to get DOWN
//      A) it didn't go DOWN for 12msec
//          ERR => show error, wait 1sec and then goto S0
//      B) it went DOWN in less then 8msec
//          ERR => show error, wait 1sec and then goto S0
//      C) it went DOWN within given timeframe
//          OK => goto S3D

// S3D) pull COM1 DOWN for 15msec and check what is on line after that
//      A) it is still low (DOWN)
//          ERR => show error, wait 1sec and then goto S0
//      B) it is high (UP)
//          OK => goto S3E

// S3E) set COM1 to 9600bod,def, and wait for 250msec + 2bytes time to receive
//      A) received 0x5A and 0xA5
//          OK => goto S3F
//      B) received some trash or nothing
//          ERR => show error, wait 1sec and then goto S0

// S3F) send 0x2E and 0xD1, switch serial to required speed (COM0 speed * 4) and wait for 2bytes
//      A) received 0x5B and 0xA4
//          OK => now we have to serve - listening to you my master! goto S5????
//      B) received some trash or nothing
//          ERR => show error, wait 1sec and then goto S0


// S4) <----- here we start real JOB!


void loop() {
  // put your main code here, to run repeatedly:

} 
