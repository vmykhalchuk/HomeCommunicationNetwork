// purpose of this sketch is to send beacon signals every 8 seconds

#include <SPI.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "RF24.h"

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);
byte addressMaster[6] = "MBank";
byte addressSlave[6] = "1Bank";

const byte pin_led = 13;

void setup() {
  pinMode(pin_led, OUTPUT);
  Serial.begin(9600);
  
  radio.begin();
  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_LOW); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(118); // 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  radio.openWritingPipe(addressMaster);
  radio.openReadingPipe(1, addressSlave);

  // Start the radio listening for data
  //radio.startListening();

  Serial.println(F("Banka Initialized"));
  digitalWrite(pin_led, HIGH);
  delay(2000);
  digitalWrite(pin_led, LOW);
  delay(2000);
}

const byte interruptPin = 2;

void pinInterruptRoutine(void)
{
  /* We detach the interrupt to stop it from 
   * continuously firing while the interrupt pin
   * is low.
   */
  detachInterrupt(0);
  delay(100); // primitive debouncing
}

ISR(WDT_vect)
{
}

void enterSleep(void)
{
  cli();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  /* Setup pin2 as an interrupt and attach handler. */
  attachInterrupt(digitalPinToInterrupt(interruptPin), pinInterruptRoutine, LOW); // can be LOW / CHANGE / HIGH
  sleep_enable();
  sei();
  sleep_cpu(); /* The program will continue from here. */ // it was : sleep_mode() , seems to be same functionality
  sleep_disable();
}


void loop() {
/****************** Ping Out Role ***************************/  
  {
    radio.stopListening();                                    // First, stop listening so we can talk.
    
    
    unsigned long start_time = micros();                      // Take the time, and send it. This will block until complete
    if (!radio.write(&start_time, sizeof(unsigned long))) {
      Serial.println(F("failed"));
    }
        
    radio.startListening();                                    // Now, continue listening
    Serial.println(F("listening"));
    
    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    boolean timeout = false;                                   // Set up a variable to indicate if a response was received or not
    
    while (!radio.available()) {                               // While nothing is received
      if (micros() - started_waiting_at > 200000) {            // If waited longer than 200ms, indicate timeout and exit while loop
          timeout = true;
          break;
      }
    }
        
    if (timeout) {                                             // Describe the results
        Serial.println(F("Failed, response timed out."));
    } else {
        unsigned long got_time;                                // Grab the response, compare, and send to debugging spew
        radio.read( &got_time, sizeof(unsigned long) );
        unsigned long end_time = micros();
        
        // Spew it
        Serial.print(F("Sent "));
        Serial.print(start_time);
        Serial.print(F(", Got response "));
        Serial.print(got_time);
        Serial.print(F(", Round-trip delay "));
        Serial.print(end_time-start_time);
        Serial.println(F(" microseconds"));
    }

    // Try again 3s later
    delay(3000);
  }
}

