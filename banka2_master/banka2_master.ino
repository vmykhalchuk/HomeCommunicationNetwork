#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include "RF24.h"

const byte zoomerPin = 13;

#define LOG_RX 4
#define LOG_TX 5
SoftwareSerial SoftSerial(LOG_RX, LOG_TX); 

#define GSMSerial Serial
#define LOG SoftSerial


ISR(WDT_vect)
{
  LOG.print("W");
}

typedef enum { WDT_PRSCL_16ms = 0, WDT_PRSCL_32ms, WDT_PRSCL_64ms, WDT_PRSCL_125ms, 
    WDT_PRSCL_250ms, WDT_PRSCL_500ms, WDT_PRSCL_1s, WDT_PRSCL_2s, WDT_PRSCL_4s, WDT_PRSCL_8s } edt_prescaler ;

void setupWdt(boolean enableInterrupt, uint8_t prescaler)
{
  byte wdtcsrValue = enableInterrupt ? 1<<WDIE : 0;
  wdtcsrValue |= (prescaler & 1) ? 1<<WDP0 : 0;
  wdtcsrValue |= (prescaler & 2) ? 1<<WDP1 : 0;
  wdtcsrValue |= (prescaler & 4) ? 1<<WDP2 : 0;
  wdtcsrValue |= (prescaler & 8) ? 1<<WDP3 : 0;
  
  cli();
  /*** Setup the WDT ***/
  /* Clear the reset flag. */
  MCUSR &= ~(1<<WDRF); // chapter 11.9
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE); // chapter 11.9 and scroll up
  /* set new watchdog timeout prescaler value */
  // WDP0 & WDP3 = 8sec
  // WDIE = enable interrupt (note no reset)
  //WDTCSR = 1<<WDIE | 1<<WDP0 | 1<<WDP3; /* 8.0 seconds */
  WDTCSR = wdtcsrValue;
  sei();
}

/* Hardware configuration: Set up nRF24L01 radio on SPI bus (???) plus pins 7 & 8 */
RF24 radio(7,8);
byte addressMaster[6] = "MBank";
byte addressSlave[6] = "1Bank";

boolean setupRadio(void)
{
  if (!radio.begin())
  {
    return false;
  }
  radio.stopListening();// stop to avoid reset MCU issues
  
  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_MAX); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS); // (default is RF24_1MBPS)
  radio.setChannel(118); // 2.518 Ghz - Above most Wifi Channels (default is 76)
  radio.setCRCLength(RF24_CRC_16); //(default is RF24_CRC_16)

  radio.openWritingPipe(addressSlave); // writing to Slave (Banka)
  radio.openReadingPipe(1, addressMaster);

  return true;
}

volatile boolean isRadioUp = true;
void putRadioDown()
{
  if (isRadioUp) {
    radio.powerDown();
  }
  isRadioUp = false;
}
void putRadioUp()
{
  if (!isRadioUp) {
    radio.powerUp();
  }
  isRadioUp = true;
}

void setup()
{
  setupRadio();
  
  Serial.begin(9600);
  while (!Serial) {}

  LOG.begin(9600);

  pinMode(zoomerPin, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(zoomerPin, HIGH);
    delay(1000);
    digitalWrite(zoomerPin, LOW);
    delay(1000);
  }
  
  setupWdt(true, WDT_PRSCL_8s);

  LOG.println("Initialization complete.");
  for (int i = 0; i < 5; i++) {
    wdt_reset();
    digitalWrite(zoomerPin, HIGH);
    delay(500);
    wdt_reset();
    digitalWrite(zoomerPin, LOW);
    delay(500);
    delay(2000);
    LOG.print('D');
  }

  LOG.println(); LOG.println("Starting business, now!");
  // Start the radio listening for data
  radio.startListening();
}

#define CR '\x0D' // CR; CR+LF; LF
#define LF '\x0A'
#define GSM_RX_BUF_SIZE 250
volatile char gsm_rx_buf[GSM_RX_BUF_SIZE];
volatile byte gsm_rx_buf_size;
volatile bool gsm_rx_buf_full_line = false;
volatile bool gsm_rx_buf_overflow = false;
volatile bool gsm_rx_buf_line_cr = false;

volatile byte transmission[9];
volatile bool readingRadioData = false;

const char GSM_OK[] = {'O', 'K'};
const char GSM_CPIN_READY[] = {'+','C','P','I','N',':',' ','R','E','A','D','Y'};
const char GSM_CALL_READY[] = {'C','a','l','l',' ','R','e','a','d','y'};
const char GSM_SMS_READY[] = {'S','M','S',' ','R','e','a','d','y'};

const char GSM_CMGS[] = {'+','C','M','G','S',':',' '};

void processGsmLine_debug()
{
  LOG.println();
  LOG.print(" GSM:");
  LOG.print(gsm_rx_buf_size);
  if (gsm_rx_buf_size > 0) {
    LOG.print('|');
    for (byte i = 0; i < gsm_rx_buf_size; i++) {
      LOG.print(gsm_rx_buf[i]);
    }
    LOG.print('|');
    LOG.print((byte)gsm_rx_buf[0], HEX);
    if (gsm_rx_buf_size > 1)
      { LOG.print('-'); LOG.print((byte)gsm_rx_buf[gsm_rx_buf_size - 1], HEX); }
  }
  LOG.println('|');
  //  { LOG.print(':');LOG.print((byte)gsm_rx_buf[0], HEX); }
  //if (gsm_rx_buf_size > 1)
  //  { LOG.print('-'); LOG.print(gsm_rx_buf[gsm_rx_buf_size - 1]); }
  LOG.flush();
}

void processRadioTransmission_debug()
{
  LOG.println();
  LOG.print(" ID:"); LOG.print(transmission[0], HEX);// Banka(R) ID
  LOG.print(" T:");  LOG.print(transmission[1], HEX);// 0- normal; 1- startup; 2- wdt overrun transmission
  LOG.print(" C:");  LOG.print(transmission[2], HEX);// Communication No
  LOG.print(" F:");  LOG.print(transmission[3], HEX);// Failed attempts to deliver this communication
  LOG.print(" W:");  LOG.print(transmission[4], HEX);// WDT overruns
  LOG.print(" M:");  LOG.print(transmission[5], HEX);// Mag State
  LOG.print(" L:");  LOG.print(transmission[6], HEX);// Light State
  LOG.print(" A:");  LOG.print(transmission[7], HEX);// Port A interrupts
  LOG.print(" B:");  LOG.print(transmission[8], HEX);// Port B interrupts
  LOG.println();
  LOG.flush();
}

bool gsmIsLine(const char* templateStr, int templateSize)
{
  if (gsm_rx_buf_size != templateSize) {
    return false;
  }
  return gsmIsLineStartsWith(templateStr, templateSize);
}

bool gsmIsLineStartsWith(const char* templateStr, int templateSize)
{
  if (gsm_rx_buf_size < templateSize) {
    return false;
  }
  for (byte i = 0; i < templateSize; i++) {
    if (gsm_rx_buf[i] != *(templateStr+i))
      return false;
  }
  return true;
}

int gsmReadLineInteger(int templateSize)
{
  if (gsm_rx_buf_size > (templateSize+3)) {
    return -1;
  }
  int res = 0;
  for (byte i = 0; i < (gsm_rx_buf_size-templateSize); i++) {
    char c = gsm_rx_buf[gsm_rx_buf_size-1-i];
    LOG.print('['); LOG.print(c); LOG.print(']');
    if (c < '0' || c > '9') {
      return -1;
    }
    byte d = (byte)c - (byte)'0';
    LOG.print('['); LOG.print(d); LOG.print(']');
    int decPow = 1; for (int j = 0; j < i; j++) decPow *= 10;
    res += decPow * d;
    LOG.print('['); LOG.print(decPow); LOG.print(']');
  }
  return res;
}

void processGsmLine()
{
  if (gsmIsLine(&GSM_OK[0], sizeof(GSM_OK))) {
    LOG.println("OK OK OK!!!!");
  }
  if (gsmIsLineStartsWith(&GSM_CMGS[0], sizeof(GSM_CMGS))) {
    LOG.println("OK CMGS OK!!!!");
    LOG.println(); LOG.print("HURA: "); LOG.println(gsmReadLineInteger(sizeof(GSM_CMGS)));
  }
  processGsmLine_debug();
}

void processRadioTransmission()
{
  
}

void loop()
{
  if (gsm_rx_buf_full_line) {
    // gsm line received - read it and process
    processGsmLine();
    resetGsmRxBufAfterLineRead();
  }
  
  if (!readingRadioData && radio.available()) {
    radio.readUnblockedStart(&transmission, sizeof(transmission));
    readingRadioData = true;
  } else if (readingRadioData && radio.readUnblockedFinished()) {
    // read transmission from radio - its ready
    processRadioTransmission();
    readingRadioData = false;
  }
}

void resetGsmRxBufAfterLineRead()
{
  cli();
  gsm_rx_buf_size = 0;
  gsm_rx_buf_full_line = false;
  gsm_rx_buf_overflow = false;
  gsm_rx_buf_line_cr = false;
  sei();
}

volatile int z = 0;
void serialEvent()
{
  if (gsm_rx_buf_full_line) return; // we have to wait before line is processed

  if (z++ > 10) // TODO set it to 1000
  {
    LOG.print('!'); LOG.flush();
    if (gsm_rx_buf_line_cr) {
      gsm_rx_buf_full_line = true;
      return;
    }
  }
  
  while (GSMSerial.available())
  {
    z = 0;
    char c = (char)GSMSerial.peek();
    //LOG.print('.');
    
    if (gsm_rx_buf_line_cr) {
      if (c == LF) {
        GSMSerial.read();
      }
      gsm_rx_buf_full_line = true;
      break; // first prev line has to be processed
    }

    GSMSerial.read();
    if (c == CR) {
      gsm_rx_buf_full_line = true; // TODO this is workaround
      //gsm_rx_buf_line_cr = true;
      break; //continue;
    } else if (c == LF) {
      gsm_rx_buf_full_line = true;
      break;
    } else {
      // just general letter
      if (gsm_rx_buf_size >= GSM_RX_BUF_SIZE) {
        gsm_rx_buf_overflow = true;
      } else {
        gsm_rx_buf[gsm_rx_buf_size] = c;
        gsm_rx_buf_size++;
      }
    }
  }
}

