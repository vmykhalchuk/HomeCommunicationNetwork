#include <SPI.h>
#include "RF24.h"


const byte THIS_REPEATER_NO = 2; // 1, 2, ...
#define DEBUG Serial

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);
#define BANKA_TRANSMISSION_SIZE 12
volatile byte transmission[BANKA_TRANSMISSION_SIZE];

#define BANKAS_BUF_SIZE 20
byte bankasTransmissionsBuf[BANKAS_BUF_SIZE][BANKA_TRANSMISSION_SIZE];
byte bankasTransmissionsFailures[BANKAS_BUF_SIZE];

byte currentChannel = 0xFF;
const byte addressMaster[6] = "MBank";
//const byte addressSlave[6] = "SBank";
const byte addressRepeater1[6] = "1Bank";
const byte addressRepeater2[6] = "2Bank";

boolean setupRadio(void)
{
  if (!radio.begin())
  {
    return false;
  }
  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(125); // 118 = 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  if (THIS_REPEATER_NO == 1)
  {
    radio.openReadingPipe(1, addressRepeater1);
  }
  else if (THIS_REPEATER_NO == 2)
  {
    radio.openReadingPipe(1, addressRepeater2);
  }
  else
  {
    return false;
  }
  switchChannel(0);

  return true;
}

void setup()
{
  for (int i = 0; i < BANKAS_BUF_SIZE; i++)
  {
    bankasTransmissionsBuf[i][0] = 0; // clean ID field
  }
  
  Serial.begin(57600);
  while(!Serial);

  if (!setupRadio()) {
    while(true) {
      Serial.println("Radio Initialization failed!!!");
      Serial.flush();
      delay(2000);
    }
  }

  Serial.println("Initialization complete.");
  Serial.flush();

  radio.startListening();
}

void switchChannel(byte channel)
{
  if (channel != currentChannel)
  {
    currentChannel = channel;
    if (channel == 0)
    {
      radio.openWritingPipe(addressMaster);
    }
    else if (channel == 1)
    {
      radio.openWritingPipe(addressRepeater1);
    }
    else if (channel == 2)
    {
      radio.openWritingPipe(addressRepeater2);
    }
    else
    { // error
      Serial.print("Wrong channel:"); Serial.println(channel); Serial.flush();
      delay(5000);
    }
  }
}

/*// type - 0 - normal transmission
//        1 - start-up transmission
//        2 - wdt overrun transmission
bool _transmitData(byte type)
{
  byte transmission[11];
  // fill in data from structure
  transmission[0] = BANKA_DEV_ID;
  transmission[1] = type;
  transmission[2] = _rneD.transmissionId;
  transmission[3] = _rneD.lastFailed;
  transmission[4] = _rneD.wdtOverruns;
  transmission[5] = _rneD.magSensorData;
  transmission[6] = _rneD.lightSensorData;
  transmission[7] = _rneD.portAData;
  transmission[8] = _rneD.portBData;
  transmission[9] = batteryVoltageHighByte;
  transmission[10] = batteryVoltageLowByte;

  byte channel = repeatersProcessor.getChannel();
  switchChannel(channel);
  bool txSucceeded = radio.write(&transmission, sizeof(transmission));
  repeatersProcessor.registerSendStatus(txSucceeded);
  wdt_reset();
  { // notify of send status
    if (txSucceeded) {
      digitalWrite(zoomerPin, HIGH);
      delay(100);
      digitalWrite(zoomerPin, LOW);
      wdt_reset();
    } else {
      for (int i = 0; i < 10; i++) {
        wdt_reset();
        digitalWrite(zoomerPin, HIGH);
        delay(15);
        wdt_reset();
        digitalWrite(zoomerPin, LOW);
        delay(15);
      }
    }
  }
  
  return txSucceeded;
}
*/
/// << transmission area end!
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//volatile bool readingRadioData = false;
void loop()
{
  if (radio.available())
  {
    radio.read((void*)(&transmission), sizeof(transmission));
    processRadioTransmission();
  }
}

void processRadioTransmission()
{
  radio.stopListening();
  //processRadioTransmission_debug();

  switchChannel(0);
  boolean txSucc = false;
  for (byte i = 0; i < 6; i++)
  {
    if (i >= 3)
    {
      switchChannel(THIS_REPEATER_NO == 2 ? 1 : 2); // try to send to another repeater if master is not available
    }
    transmission[11]++;
    txSucc = radio.write((void*)(&transmission), sizeof(transmission));
    if (txSucc) break;
  }
  if (!txSucc)
  {
    // now we have to add it to some queue, or just admit we failed
  }

  radio.startListening();
}

void processRadioTransmission_debug()
{
  DEBUG.println();
  DEBUG.print(" ID:"); DEBUG.print(transmission[0], HEX);// Banka(R) ID
  DEBUG.print(" T:");  DEBUG.print(transmission[1], HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  DEBUG.print(" C:");  DEBUG.print(transmission[2], HEX);// Communication No
  DEBUG.print(" F:");  DEBUG.print(transmission[3], HEX);// Failed attempts to deliver this communication
  DEBUG.print(" W:");  DEBUG.print(transmission[4], HEX);// WDT overruns
  DEBUG.print(" M:");  DEBUG.print(transmission[5], HEX);// Mag State
  DEBUG.print(" L:");  DEBUG.print(transmission[6], HEX);// Light State
  DEBUG.print(" A:");  DEBUG.print(transmission[7], HEX);// Port A interrupts
  DEBUG.print(" B:");  DEBUG.print(transmission[8], HEX);// Port B interrupts
  uint16_t bankaVccAdc = transmission[9]<<8|transmission[10];
  float bankaVcc = 1.1 / float (bankaVccAdc + 0.5) * 1024.0;
  DEBUG.print(" VCC:"); DEBUG.print(bankaVcc);// Battery Voltage
  DEBUG.print(" RC:"); DEBUG.print(transmission[11]);
  DEBUG.println();
  DEBUG.flush();
}

